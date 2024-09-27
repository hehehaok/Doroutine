#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <chrono>

#include "iomanager.h"
#include "forTest.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int sockfd;
void watch_io_read();
void do_io_write2();

// 写事件回调，只执行一次，用于判断非阻塞套接字connect成功
void do_io_write() {
    SYLAR_LOG_INFO(g_logger) << "write callback";
    int so_err;
    socklen_t len = size_t(so_err);
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_err, &len);
    if(so_err) {
        SYLAR_LOG_INFO(g_logger) << "connect fail";
        return;
    } 
    SYLAR_LOG_INFO(g_logger) << "connect success";
    KSC::IOManager::GetThis()->schedule(do_io_write2);
}

void watch_io_write() {
    SYLAR_LOG_INFO(g_logger) << "watch_io_write";
    KSC::IOManager::GetThis()->addEvent(sockfd, KSC::IOManager::WRITE, do_io_write2);
}

void do_io_write2() {
    SYLAR_LOG_INFO(g_logger) << "write callback";
    char buf[1024] = "hello~\n";
    int writelen = 0;
    write(sockfd, buf, sizeof(buf));
    KSC::IOManager::GetThis()->schedule(watch_io_write);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

// 读事件回调，每次读取之后如果套接字未关闭，需要重新添加
void do_io_read() {
    SYLAR_LOG_INFO(g_logger) << "read callback";
    char buf[1024] = {0};
    int readlen = 0;
    readlen = read(sockfd, buf, sizeof(buf));
    if(readlen > 0) {
        buf[readlen] = '\0';
        SYLAR_LOG_INFO(g_logger) << "read " << readlen << " bytes, read: " << buf;
    } else if(readlen == 0) {
        SYLAR_LOG_INFO(g_logger) << "peer closed";
        close(sockfd);
        return;
    } else {
        SYLAR_LOG_INFO(g_logger) << "err, errno=" << errno << ", errstr=";
        close(sockfd);
        return;
    }
    // read之后重新添加读事件回调，这里不能直接调用addEvent，因为在当前位置fd的读事件上下文还是有效的，直接调用addEvent相当于重复添加相同事件
    KSC::IOManager::GetThis()->schedule(watch_io_read);
}

void watch_io_read() {
    SYLAR_LOG_INFO(g_logger) << "watch_io_read";
    KSC::IOManager::GetThis()->addEvent(sockfd, KSC::IOManager::READ, do_io_read);
}

void test_io() {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(9999);
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr.s_addr);

    int rt = connect(sockfd, (const sockaddr*)&servaddr, sizeof(servaddr));
    if(rt != 0) {
        if(errno == EINPROGRESS) {
            SYLAR_LOG_INFO(g_logger) << "EINPROGRESS";
            // 注册写事件回调，只用于判断connect是否成功
            // 非阻塞的TCP套接字connect一般无法立即建立连接，要通过套接字可写来判断connect是否已经成功
            KSC::IOManager::GetThis()->addEvent(sockfd, KSC::IOManager::WRITE, do_io_write);
            // 注册读事件回调，注意事件是一次性的
            KSC::IOManager::GetThis()->addEvent(sockfd, KSC::IOManager::READ, do_io_read);
        } else {
            SYLAR_LOG_INFO(g_logger) << "connect error, errno:" << errno << ", errstr:";
        }
    } else {
        SYLAR_LOG_INFO(g_logger) << "else, errno:" << errno << ", errstr:";
    }
}

void test_iomanager() {
    KSC::IOManager iom;
    // KSC::IOManager iom(10); // 演示多线程下IO协程在不同线程之间切换
    iom.schedule(test_io);
}

int main(int argc, char *argv[]) {
    // KSC::forTest();
    test_iomanager();
    return 0;
}