#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <event2/event.h>

void echo_read_cb(evutil_socket_t fd, short events, void *arg) {
    char buf[1024];
    int len;
    len = recv(fd, buf, sizeof(buf)-1, 0);
    if (len <= 0) {
        // 发生错误或连接关闭，关闭连接并释放事件资源
        close(fd);
        event_free((struct event *)arg);
        return;
    }
    buf[len] = '\0';
    // printf("接收到消息： %s\n", buf); // 性能测试时关掉
    send(fd, buf, len, 0);
}

// 接受连接回调函数
void accept_conn_cb(evutil_socket_t listener, short event, void *arg) {
    struct event_base *base = (struct event_base *)arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr *)&ss, &slen);
    if (fd < 0) {
        perror("accept");
    } else if (fd > FD_SETSIZE) {
        close(fd);
    } else {

        // 创建一个新的事件结构体
        struct event *ev = event_new(base, fd, EV_READ | EV_PERSIST, echo_read_cb, (void *)ev);
        event_assign(ev, base, fd, EV_READ | EV_PERSIST, echo_read_cb, (void *)ev);
        if (!ev) {
            perror("Failed to create client event");
            close(fd);
            return;
        }
        // 将新的事件添加到事件循环中
        event_add(ev, NULL);
    }
}

int main() {
    int portno = 8080;

    // 创建监听套接字
    int sock_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_listen_fd < 0) {
        perror("Error creating socket..\n");
        return -1;
    }
    int yes = 1;
    // 设置 SO_REUSEADDR 选项
    setsockopt(sock_listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(portno);
    inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr.s_addr);

    // 绑定并监听
    if (bind(sock_listen_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Error binding socket..\n");
        close(sock_listen_fd);
        return -1;
    }
    if (listen(sock_listen_fd, 5) < 0) {
        perror("Error listening..\n");
        close(sock_listen_fd);
        return -1;
    }

    struct event_base *base;
    struct event *listener;

    // 初始化 Libevent 库
    base = event_base_new();
    if (!base) {
        perror("Failed to create event base");
        close(sock_listen_fd);
        return -1;
    }

    // 创建一个监听事件
    listener = event_new(base, sock_listen_fd, EV_READ | EV_PERSIST, accept_conn_cb, (void *)base);
    if (!listener) {
        perror("Failed to create listener event");
        event_base_free(base);
        close(sock_listen_fd);
        return -1;
    }

    // 将监听事件添加到事件循环
    if (event_add(listener, NULL) == -1) {
        perror("event_add");
        event_free(listener);
        event_base_free(base);
        close(sock_listen_fd);
        return -1;
    }

    // 开始事件循环
    event_base_dispatch(base);

    // 清理资源
    event_free(listener);
    close(sock_listen_fd);
    event_base_free(base);

    return 0;
}