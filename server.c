#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 32
#define BUF_SIZE 512
#define BACKLOG 512

void fatal(char* msg);
int setnonblocking(int sockfd);
void epoll_ctl_add(int epfd, int fd, uint32_t events);

int main() {
  // 1
  // 创建 socket 实例并监听指定端口
  int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in srv_addr;
  bzero(&srv_addr, sizeof(struct sockaddr_in));
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_addr.s_addr = INADDR_ANY;
  srv_addr.sin_port = htons(8088);
  bind(listen_sock, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
  setnonblocking(listen_sock);
  listen(listen_sock, BACKLOG);

  // 2
  // 创建 epoll 并订阅 listen_sock 的事件：
  // - EPOLLIN 可读
  // - EPOLLOUT 可写
  // - 触发方式为：EPOLLET 边缘触发（edge-triggered）
  int epfd = epoll_create(MAX_EVENTS);
  epoll_ctl_add(epfd, listen_sock, EPOLLIN | EPOLLOUT | EPOLLET);

  while (1) {
    struct epoll_event events[MAX_EVENTS];

    // 3
    // 监听事件，
    int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);

    for (int i = 0; i < nfds; i++) {
      if (events[i].data.fd == listen_sock) {
        // 4
        // 接收客户端连接
        int conn_sock = accept(listen_sock, NULL, NULL);
        printf("new conn: %d\n", conn_sock);

        // 5
        // 设置连接为非阻塞模式
        setnonblocking(conn_sock);

        // 6
        // 将连接加入到 epoll 实例中以订阅其事件：
        // - EPOLLRDHUP 关闭
        // - EPOLLHUP 非正常关闭
        epoll_ctl_add(epfd, conn_sock,
                      EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP);
      } else if (events[i].events & EPOLLIN) {
        while (1) {
          // 7
          // 因为这个例子使用的是边缘触发，所以一次读取完内核中缓冲区的内容
          char buf[BUF_SIZE];
          bzero(buf, sizeof(buf));
          int n = read(events[i].data.fd, buf, sizeof(buf));
          if (n <= 0) {
            break;
          } else {
            printf("client #%d sent: %s\n", events[i].data.fd, buf);
            write(events[i].data.fd, buf, n);
          }
        }
      }
      if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
        // 8
        // 关闭连接
        printf("close client: %d\n", events[i].data.fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
        close(events[i].data.fd);
        continue;
      }
    }
  }
  return 0;
}

void fatal(char* msg) {
  fprintf(stderr, "%s errno: %d\n", msg, errno);
  exit(1);
}

int setnonblocking(int sockfd) {
  if (fcntl(sockfd, F_SETFD, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) == -1)
    return -1;
  return 0;
}

void epoll_ctl_add(int epfd, int fd, uint32_t events) {
  struct epoll_event ev;
  ev.events = events;
  ev.data.fd = fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1)
    fatal("unable to epoll_ctl_add");
}
