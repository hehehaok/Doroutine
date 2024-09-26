#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>

#include "hook.h"
#include "iomanager.h"

void test_sleep() {
    std::cout << "test_sleep begin" << std::endl;
    KSC::IOManager* iom = KSC::IOManager::GetThis();

    iom->schedule([] {
        sleep(2);
        std::cout << "sleep 2" << std::endl;
    });

    iom->schedule([] {
        sleep(3);
        std::cout << "sleep 3" << std::endl;
    });

    std::cout << "test_sleep end" << std::endl;
}

void test_sock() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "36.152.44.96", &addr.sin_addr.s_addr);

    std::cout << "begin connect" << std::endl;
    int rt = connect(sock, (const sockaddr*)&addr, sizeof(addr));
    std::cout << "connect rt=" << rt << " errno=" << errno;

    if(rt) {
        return;
    }

    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(sock, data, sizeof(data), 0);
    std::cout << "send rt=" << rt << " errno=" << errno;

    if(rt <= 0) {
        return;
    }

    std::string buff;
    buff.resize(4096);

    rt = recv(sock, &buff[0], buff.size(), 0);
    std::cout << "recv rt=" << rt << " errno=" << errno;

    if(rt <= 0) {
        return;
    }

    buff.resize(rt);
    std::cout << buff;
}

int main() {
    KSC::IOManager iom;
    iom.schedule(test_sock);

    std::cout << "main end" << std::endl;
    return 0;
}