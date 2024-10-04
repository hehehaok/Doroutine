#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>

#include "hook.h"
#include "iomanager.h"
#include "forTest.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_sleep() {
    SYLAR_LOG_INFO(g_logger) << "test_sleep begin";
    KSC::IOManager* iom = KSC::IOManager::GetThis();

    iom->schedule([] {
        sleep(2);
        SYLAR_LOG_INFO(g_logger) << "sleep 2";
    });

    iom->schedule([] {
        sleep(3);
        SYLAR_LOG_INFO(g_logger) << "sleep 3";
    });

    SYLAR_LOG_INFO(g_logger) << "test_sleep end";
}

void test_sock() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "36.152.44.96", &addr.sin_addr.s_addr);

    SYLAR_LOG_INFO(g_logger) << "begin connect";
    int rt = connect(sock, (const sockaddr*)&addr, sizeof(addr));
    SYLAR_LOG_INFO(g_logger) << "connect rt=" << rt << " errno=" << errno;

    if(rt) {
        return;
    }

    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(sock, data, sizeof(data), 0);
    SYLAR_LOG_INFO(g_logger) << "send rt=" << rt << " errno=" << errno;

    if(rt <= 0) {
        return;
    }

    std::string buff;
    buff.resize(4096);

    rt = recv(sock, &buff[0], buff.size(), 0);
    SYLAR_LOG_INFO(g_logger) << "recv rt=" << rt << " errno=" << errno;

    if(rt <= 0) {
        return;
    }

    buff.resize(rt);
    SYLAR_LOG_INFO(g_logger) << buff;
}

int main() {
    KSC::setLogLevelDebug();
    KSC::IOManager iom;
    iom.schedule(test_sleep);

    SYLAR_LOG_INFO(g_logger) << "main end";
    return 0;
}