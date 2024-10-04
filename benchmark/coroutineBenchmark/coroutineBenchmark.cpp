#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>

#include "iomanager.h"
#include "forTest.h"

#define PORT 8080
#define ADDR "127.0.0.1"

static int listenFd = -1;
static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void testAccept();

int setNonblock(int fd) {
    int oldOptions = fcntl(fd, F_GETFL);
    int ret = fcntl(fd, F_SETFL, oldOptions | O_NONBLOCK);
    if (ret < 0) {
        SYLAR_LOG_ERROR(g_logger) << "Error setting fd nonblock..";
    }
    return ret;
}

int socketInit() {
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) {
        SYLAR_LOG_ERROR(g_logger) << "Error creating socket..";
        return -1;
    }
    int yes = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setNonblock(listenFd);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    inet_pton(AF_INET, ADDR, &sin.sin_addr.s_addr);

    if (bind(listenFd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        SYLAR_LOG_ERROR(g_logger) << "Error binding socket..";
        close(listenFd);
        return -1;
    }
    if (listen(listenFd, 5) < 0) {
        SYLAR_LOG_ERROR(g_logger) << "Error listening..";
        close(listenFd);
        return -1;
    }
}

void watchListenFd() {
    KSC::IOManager::GetThis()->addEvent(listenFd, KSC::IOManager::READ, testAccept);
    SYLAR_LOG_INFO(g_logger) << "watchListenFd over!";
}

void fdReadCallback(int fd) {
    SYLAR_LOG_INFO(g_logger) << "文件标识符" << fd << "的读事件触发!";
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    while (true) {
        int ret = recv(fd, buffer, sizeof(buffer)-1, 0);
        if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            SYLAR_LOG_INFO(g_logger) << "本轮数据读取完毕!";
            KSC::IOManager::GetThis()->addEvent(fd, KSC::IOManager::READ, std::bind(fdReadCallback, fd));
            break;
        }
        if (ret <= 0) {
            SYLAR_LOG_INFO(g_logger) << "客户端断开连接";
            close(fd);
            break;
        }
        buffer[ret] = '\0';
        SYLAR_LOG_INFO(g_logger) << "接收到消息:\n" << buffer;
        send(fd, buffer, ret, 0);
    }
}

void testAccept() {
    struct sockaddr_in addr; // maybe sockaddr_un;
    memset(&addr, 0, sizeof(addr));
    socklen_t len = sizeof(addr);
    int fd = accept(listenFd, (struct sockaddr *)&addr, &len);
    if (fd < 0) {
        SYLAR_LOG_ERROR(g_logger) << "fd = " << fd << "accept false";
    } else {
        SYLAR_LOG_INFO(g_logger) << "accept success! The addr:port of client is " << addr.sin_addr.s_addr << ":" << addr.sin_port;
        setNonblock(fd);
        KSC::IOManager::GetThis()->addEvent(fd, KSC::IOManager::READ, std::bind(fdReadCallback, fd));
    }
    KSC::IOManager::GetThis()->schedule(watchListenFd);
    SYLAR_LOG_INFO(g_logger) << "testAccept over!";
}

int main() {
    KSC::setLogDisable(); // 性能测试时关掉日志
    if (socketInit() < 0) {
        return -1;
    }
    KSC::IOManager iom;
    KSC::IOManager::GetThis()->schedule(watchListenFd);
    return 0;
}
