#include <dlfcn.h>
#include <iostream>
#include <memory>
#include <stdarg.h>

#include "doroutine.h"
#include "iomanager.h"
#include "fdManager.h"
#include "hook.h"

namespace KSC {

static thread_local bool st_hookEnable = false;

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)

void hookInit() {
    static bool is_inited = false;
    if (is_inited) {
        return;
    }
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX);
#undef XX
}

static uint64_t s_connect_timeout = 5000;

struct _HookIniter {
    _HookIniter() {
        hookInit();
    }
};

static _HookIniter s_hookIniter;

bool isHookEnable() {
    return st_hookEnable;
}

void setHookEnable(bool flag) {
    st_hookEnable = flag;
}

}; // namespace KSC

struct timerInfo {
    int cancelled = 0;
};

template<typename originFunc, typename... Args>
static ssize_t do_io(int fd, originFunc fun, const char* hook_fun_name,
        uint32_t event, int timeout_so, Args&&... args) {
    if (!KSC::st_hookEnable) {
        return fun(fd, std::forward<Args>(args)...);
    }

    KSC::FdCtx::ptr ctx = KSC::FdManager::GetInstance()->get(fd);
    if (!ctx) {
        return fun(fd, std::forward<Args>(args)...);
    }

    if (ctx->isClose()) {
        errno = EBADF;
        return -1;
    }

    if (!ctx->isSocket() || ctx->getUserNonblock()) {
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->getTimeout(timeout_so);
    std::shared_ptr<timerInfo> tinfo = std::make_shared<timerInfo>();

retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while(n == -1 && errno == EINTR) {
        n = fun(fd, std::forward<Args>(args)...);
    }

    if (n == -1 && errno == EAGAIN) {
        KSC::IOManager* iom = KSC::IOManager::GetThis();
        KSC::Timer::ptr timer;
        std::weak_ptr<timerInfo> winfo(tinfo);

        if (to != (uint64_t)-1) {
            timer = iom->addConditionalTimer(to, [winfo, fd, iom, event]() {
                auto t = winfo.lock();
                if (!t || t->cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, (KSC::IOManager::Event)(event));
            }, winfo);
        }

        int rt = iom->addEvent(fd, (KSC::IOManager::Event)(event));
        if (rt == 0) {
            KSC::Doroutine::GetThis()->yield();
            if (timer) {
                timer->cancel();
            }
            if (tinfo->cancelled) {
                errno = tinfo->cancelled;
                return -1;
            }
            goto retry;
        } else {
            if (timer) {
                timer->cancel();
            }
            std::cout << hook_fun_name << " addEvent(" << fd << ", " << event << ") wrong";
        }
    }
    return n;
}

extern "C" {
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX

unsigned int sleep(unsigned int seconds) {
    if (!KSC::st_hookEnable) {
        return sleep_f(seconds);
    }

    KSC::Doroutine::ptr doroutine = KSC::Doroutine::GetThis();
    KSC::IOManager *iom = KSC::IOManager::GetThis();

    using schedulePtr = void(KSC::Scheduler::*)(KSC::Doroutine::ptr, int); // 由于schedule是模板函数，因此此处要特化模板后才能进行bind
    iom->addTimer(seconds * 1000, std::bind((schedulePtr)&KSC::IOManager::schedule, iom, doroutine, -1));

    KSC::Doroutine::GetThis()->yield();
    return 0;
}

int usleep(useconds_t usec) {
    if (!KSC::st_hookEnable) {
        return usleep_f(usec);
    }

    KSC::Doroutine::ptr doroutine = KSC::Doroutine::GetThis();
    KSC::IOManager *iom = KSC::IOManager::GetThis();

    using schedulePtr = void(KSC::Scheduler::*)(KSC::Doroutine::ptr, int); // 由于schedule是模板函数，因此此处要特化模板后才能进行bind
    iom->addTimer(usec / 1000, std::bind((schedulePtr)&KSC::IOManager::schedule, iom, doroutine, -1));

    KSC::Doroutine::GetThis()->yield();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!KSC::st_hookEnable) {
        return nanosleep_f(req, rem);
    }

    int timeoutMs = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    KSC::Doroutine::ptr doroutine = KSC::Doroutine::GetThis();
    KSC::IOManager *iom = KSC::IOManager::GetThis();

    using schedulePtr = void(KSC::Scheduler::*)(KSC::Doroutine::ptr, int); // 由于schedule是模板函数，因此此处要特化模板后才能进行bind
    iom->addTimer(timeoutMs, std::bind((schedulePtr)&KSC::IOManager::schedule, iom, doroutine, -1));

    KSC::Doroutine::GetThis()->yield();
    return 0;
}

int socket(int domain, int type, int protocol) {
    if (!KSC::st_hookEnable) {
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if (fd == -1) {
        return fd;
    }
    KSC::FdManager::GetInstance()->get(fd, true);
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms) {
    if (!KSC::st_hookEnable) {
        return connect_f(fd, addr, addrlen);
    }

    KSC::FdCtx::ptr ctx = KSC::FdManager::GetInstance()->get(fd);
    if (!ctx || ctx->isClose()) {
        errno = EBADF;
        return -1;
    }

    if (!ctx->isSocket()) {
        return connect_f(fd, addr, addrlen);
    }

    if (ctx->getUserNonblock()) {
        return connect_f(fd, addr, addrlen);
    }

    int n = connect_f(fd, addr, addrlen);
    if (n == 0) {
        return 0;
    } else if (n != -1 || errno != EINPROGRESS) {
        return n;
    }

    KSC::IOManager *iom = KSC::IOManager::GetThis();
    KSC::Timer::ptr timer;
    std::shared_ptr<timerInfo> tinfo = std::make_shared<timerInfo>();
    std::weak_ptr<timerInfo> winfo(tinfo);

    if (timeout_ms != (uint64_t)-1) {
        iom->addConditionalTimer(timeout_ms, [winfo, fd, iom]{
            auto tinfo = winfo.lock();
            if (!tinfo || tinfo->cancelled) {
                return;
            }
            tinfo->cancelled = ETIMEDOUT;
            iom->cancelEvent(fd, KSC::IOManager::Event::WRITE);
        }, winfo);
    }

    int rt = iom->addEvent(fd, KSC::IOManager::Event::WRITE); // 注意这里没有传入回调，则回调默认为当前协程
    if (rt == 0) {
        KSC::Doroutine::GetThis()->yield(); 
        // 当前协程切出，此时协程切回有两种情况：
        // 1.在超时前fd就可写了，触发fd的写事件切回
        // 2.超出超时时间，定时器取消事件，由于取消事件时会触发一次事件回调，因此也会导致协程切回
        // 对于情况1，在定时器超时之前切回当前协程，由于当前协程执行完才能执行定时器的超时回调，此时tinfo已经被销毁了
        // 所以在定时器的超时回调中会命中if(!info || tinfo->cancelld)直接返回，相当于超时回调啥也没做
        // 对于情况2，超时后执行超时回调，将tinfo->cancelled设置超时错误标志，同时取消事件，此时切回当前协程会命中
        // tinfo->cancelled，返回-1并标识错误为超时
        // 太妙了！
        if (timer) {
            timer->cancel();
        }
        if (tinfo->cancelled) {
            errno = tinfo->cancelled;
            return -1;
        }
    } else {
        if (timer) {
            timer->cancel();
        }
        std::cout << "connect addEvent(" << fd << ", WRITE) error" << std::endl;
    }

    int error = 0;
    socklen_t len = sizeof(int);
    if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }
    if(!error) {
        return 0;
    } else {
        errno = error;
        return -1;
    }    
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return connect_with_timeout(sockfd, addr, addrlen, KSC::s_connect_timeout);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
    int fd = do_io(s, accept_f, "accept", KSC::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if (fd >= 0) {
        KSC::FdManager::GetInstance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count) {
    return do_io(fd, read_f, "read", KSC::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, readv_f, "readv", KSC::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return do_io(sockfd, recv_f, "recv", KSC::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    return do_io(sockfd, recvfrom_f, "recvfrom", KSC::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", KSC::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return do_io(fd, write_f, "write", KSC::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, writev_f, "writev", KSC::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags) {
    return do_io(s, send_f, "send", KSC::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    return do_io(s, sendto_f, "sendto", KSC::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
    return do_io(s, sendmsg_f, "sendmsg", KSC::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
    if (!KSC::st_hookEnable) {
        return close_f(fd);
    }

    KSC::FdCtx::ptr ctx = KSC::FdManager::GetInstance()->get(fd);
    if (ctx) {
        auto iom = KSC::IOManager::GetThis();
        if (iom) {
            iom->cancelAll(fd);
        }
        KSC::FdManager::GetInstance()->delFd(fd);
    }
    return close_f(fd);
}

int fcntl(int fd, int cmd, ... /* arg */ ) {
    va_list va;
    va_start(va, cmd);
    switch(cmd) {
        case F_SETFL:
            {
                int arg = va_arg(va, int);
                va_end(va);
                KSC::FdCtx::ptr ctx = KSC::FdManager::GetInstance()->get(fd);
                if(!ctx || ctx->isClose() || !ctx->isSocket()) {
                    return fcntl_f(fd, cmd, arg);
                }
                ctx->setUserNonblock(arg & O_NONBLOCK);
                if(ctx->getSysNonblock()) {
                    arg |= O_NONBLOCK;
                } else {
                    arg &= ~O_NONBLOCK;
                }
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFL:
            {
                va_end(va);
                int arg = fcntl_f(fd, cmd);
                KSC::FdCtx::ptr ctx = KSC::FdManager::GetInstance()->get(fd);
                if(!ctx || ctx->isClose() || !ctx->isSocket()) {
                    return arg;
                }
                if(ctx->getUserNonblock()) {
                    return arg | O_NONBLOCK;
                } else {
                    return arg & ~O_NONBLOCK;
                }
            }
            break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
            {
                int arg = va_arg(va, int);
                va_end(va);
                return fcntl_f(fd, cmd, arg); 
            }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct flock* arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

int ioctl(int d, unsigned long int request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if(FIONBIO == request) {
        bool user_nonblock = !!*(int*)arg;
        KSC::FdCtx::ptr ctx = KSC::FdManager::GetInstance()->get(d);
        if(!ctx || ctx->isClose() || !ctx->isSocket()) {
            return ioctl_f(d, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if (!KSC::st_hookEnable) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if (level == SOL_SOCKET) {
        if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            KSC::FdCtx::ptr ctx = KSC::FdManager::GetInstance()->get(sockfd);
            if (ctx) {
                const timeval* v = (const timeval*)optval;
                ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}; // extern "C"