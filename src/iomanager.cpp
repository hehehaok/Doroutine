#include <iostream>
#include <sys/epoll.h>
#include <fcntl.h> 
#include <unistd.h>
#include <string.h>

#include "log.h"
#include "iomanager.h"

namespace KSC {

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

IOManager::FdContext::EventContext &IOManager::FdContext::getEventContext(Event event) {
    switch (event) {
    case Event::READ:
        return read;
    case Event::WRITE:
        return write;
    default:
        SYLAR_LOG_DEBUG(g_logger) << "invalid argument";
        break;
    }
}

void IOManager::FdContext::resetEventContext(EventContext &ctx) {
    ctx.scheduler = nullptr;
    ctx.doroutine.reset();
    ctx.func = nullptr;
}

void IOManager::FdContext::triggerEvent(Event event) {
    if (!(events & event)) {
        SYLAR_LOG_DEBUG(g_logger) << "triggerEvent wrong!";
    }

    EventContext &ctx = getEventContext(event);
    if (ctx.func) {
        ctx.scheduler->schedule(ctx.func);
    } else {
        ctx.scheduler->schedule(ctx.doroutine);
    }
    events = (Event)(events & ~event);
    resetEventContext(ctx);
}

IOManager::IOManager(size_t threads, bool userCaller, const std::string &name) 
    : Scheduler(threads, userCaller, name) {
    
    m_epfd = epoll_create(5000);
    if (m_epfd <= 0) {
        SYLAR_LOG_DEBUG(g_logger) << "epoll_create wrong!";
    }

    int rt = pipe(m_tickleFds);
    if (rt == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "pipe wrong!";
    }

    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFds[0];

    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
    if (rt == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "fcntl wrong!";
    }

    rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    if (rt == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "epoll_ctl wrong!";
    }

    contextResize(32);
    start();
}

IOManager::~IOManager() {
    stop();
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    for (size_t i = 0; i < m_fdContexts.size(); i++) {
        if (m_fdContexts[i]) {
            delete m_fdContexts[i];
        }
    }
}

int IOManager::addEvent(int fd, Event event, std::function<void()> func) {
    FdContext *fdCtx = nullptr;
    readMtx lck(m_rwmtx);
    if ((int)m_fdContexts.size() > fd) {
        fdCtx = m_fdContexts[fd];
        lck.unlock();
    } else {
        lck.unlock();
        writeMtx lck2(m_rwmtx);
        contextResize(fd * 1.5);
        fdCtx = m_fdContexts[fd];
    }

    std::unique_lock<std::mutex> lck2(fdCtx->mtx);
    int op = fdCtx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epEvent;
    epEvent.events = EPOLLET | fdCtx->events | event;
    epEvent.data.ptr = fdCtx;

    int rt = epoll_ctl(m_epfd, op, fd, &epEvent);
    if (rt == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "epoll ctl wrong!";
        return -1;
    }

    ++m_pendingEventCount;

    fdCtx->events = (Event)(fdCtx->events | event);
    FdContext::EventContext &eventCtx = fdCtx->getEventContext(event);

    eventCtx.scheduler = Scheduler::GetThis();
    if (func) {
        eventCtx.func.swap(func);
    } else {
        eventCtx.doroutine = Doroutine::GetThis();
    }
    return 0;
}

bool IOManager::delEvent(int fd, Event event) {
    readMtx lck(m_rwmtx);
    if ((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext *fdCtx = m_fdContexts[fd];
    lck.unlock();

    std::unique_lock<std::mutex> lck2(fdCtx->mtx);
    if (!(fdCtx->events & event)) {
        SYLAR_LOG_DEBUG(g_logger) << "no event to delete!";
        return false;
    }

    Event newEvent = (Event)(fdCtx->events & ~event);
    int op = newEvent ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epEvent;
    epEvent.events = EPOLLET | newEvent;
    epEvent.data.ptr = fdCtx;

    int rt = epoll_ctl(m_epfd, op, fd, &epEvent);
    if (rt == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "epoll ctl wrong!";
        return false;
    }

    --m_pendingEventCount;

    fdCtx->events = newEvent;
    FdContext::EventContext &eventCtx = fdCtx->getEventContext(event);
    fdCtx->resetEventContext(eventCtx);
    return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
    readMtx lck(m_rwmtx);
    if ((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext *fdCtx = m_fdContexts[fd];
    lck.unlock();

    std::unique_lock<std::mutex> lck2(fdCtx->mtx);
    if (!(fdCtx->events & event)) {
        SYLAR_LOG_DEBUG(g_logger) << "no event to cancel!";
        return false;
    }

    Event newEvent = (Event)(fdCtx->events & ~event);
    int op = newEvent ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epEvent;
    epEvent.events = EPOLLET | newEvent;
    epEvent.data.ptr = fdCtx;

    int rt = epoll_ctl(m_epfd, op, fd, &epEvent);
    if (rt == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "epoll ctl wrong!";
        return false;
    }

    fdCtx->triggerEvent(event);
    --m_pendingEventCount;
    return true;
}

bool IOManager::cancelAll(int fd) {
    readMtx lck(m_rwmtx);
    if ((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext *fdCtx = m_fdContexts[fd];
    lck.unlock();

    std::unique_lock<std::mutex> lck2(fdCtx->mtx);
    if (!fdCtx->events) {
        SYLAR_LOG_DEBUG(g_logger) << "no event to cancel!";
        return false;
    }

    int op = EPOLL_CTL_DEL;
    epoll_event epEvent;
    epEvent.events = 0;
    epEvent.data.ptr = fdCtx;

    int rt = epoll_ctl(m_epfd, op, fd, &epEvent);
    if (rt == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "epoll ctl wrong!";
        return false;
    }

    if (fdCtx->events & Event::READ) {
        fdCtx->triggerEvent(Event::READ);
        --m_pendingEventCount;
    }
        if (fdCtx->events & Event::WRITE) {
        fdCtx->triggerEvent(Event::WRITE);
        --m_pendingEventCount;
    }
    return true;
}

IOManager *IOManager::GetThis() {
    IOManager *iom = dynamic_cast<IOManager *>(Scheduler::GetThis());
    if (iom == nullptr) {
        SYLAR_LOG_ERROR(g_logger) << "dynamic_cast fail!";
        return nullptr;
    }
    return iom;
}

void IOManager::tickle() {
    SYLAR_LOG_DEBUG(g_logger) << "tickle";
    if(!hasIdleThreads()) {
        return;
    }
    int rt = write(m_tickleFds[1], "T", 1);
    if (rt == -1) {
        SYLAR_LOG_DEBUG(g_logger) << "write wrong!";
    }
}

bool IOManager::stopping() {
    uint64_t timeout = 0;
    return stopping(timeout);
}

void IOManager::idle() {
    const uint64_t MAX_EVENTS = 256;
    epoll_event *events = new epoll_event[MAX_EVENTS];
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event *ptr) {
        delete[] ptr;
    });

    while(true) {
        uint64_t nextTimeout = 0;
        if (stopping(nextTimeout)) {
            SYLAR_LOG_DEBUG(g_logger) << "idle stop exit";
            break;
        }

        int rt = 0;
        do {
            // 默认超时时间5秒，如果下一个定时器的超时时间大于5秒，仍以5秒来计算超时，避免定时器超时时间太大时，epoll_wait一直阻塞
            static const int MAX_TIMEOUT = 5000;
            if(nextTimeout != ~0ull) {
                nextTimeout = std::min((int)nextTimeout, MAX_TIMEOUT);
            } else {
                nextTimeout = MAX_TIMEOUT;
            }
            rt = epoll_wait(m_epfd, events, MAX_EVENTS, (int)nextTimeout);
            if(rt < 0 && errno == EINTR) {
                continue;
            } else {
                break;
            }
        } while(true);

        std::vector<std::function<void()>> funcs;
        listExpiredFunc(funcs);
        if(!funcs.empty()) {
            for(const auto &func : funcs) {
                schedule(func);
            }
            funcs.clear();
        }

        for (int i = 0; i < rt; i++) {
            epoll_event &event = events[i];
            if (event.data.fd == m_tickleFds[0]) {
                uint8_t dummy[256];
                while (read(m_tickleFds[0], dummy, sizeof(dummy)) > 0)
                    ;
                continue;
            }

            FdContext *fdCtx = (FdContext *)event.data.ptr;
            std::unique_lock<std::mutex> lck(fdCtx->mtx);

            if (event.events & (EPOLLERR | EPOLLHUP)) {
                event.events |= (EPOLLIN | EPOLLOUT) & fdCtx->events;
            }

            int realEvents = NONE;
            if (event.events & EPOLLIN) {
                realEvents |= Event::READ;
            }
            if (event.events & EPOLLOUT) {
                realEvents |= Event::WRITE;
            }

            if (!(fdCtx->events & realEvents)) {
                continue;
            }

            int leftEvents = (fdCtx->events & ~realEvents);
            int op = leftEvents ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = EPOLLET | leftEvents;

            int rt2 = epoll_ctl(m_epfd, op, fdCtx->fd, &event);
            if (rt2 == -1) {
                SYLAR_LOG_DEBUG(g_logger) << "epoll ctl wrong!";
                continue;
            }

            if (realEvents & Event::READ) {
                fdCtx->triggerEvent(Event::READ);
                --m_pendingEventCount;
            }
            
            if (realEvents & Event::WRITE) {
                fdCtx->triggerEvent(Event::WRITE);
                --m_pendingEventCount;
            }
        } // end for
        Doroutine::ptr cur = Doroutine::GetThis();
        auto rawPtr = cur.get();
        cur.reset();

        rawPtr->yield();
    } // end while(true)
}

void IOManager::onTimerInsertedAtFront() {
    tickle();
}

bool IOManager::stopping(uint64_t &timeout) {
    timeout = getNextTimer();
    return timeout == ~0ull && m_pendingEventCount == 0 && Scheduler::stopping();
}

void IOManager::contextResize(size_t size) {
    m_fdContexts.resize(size);

    for (int i = 0; i < m_fdContexts.size(); i++) {
        if (!m_fdContexts[i]) {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
}
};