#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "locker/locker.h"
#include "threadpool/threadpool.h"
#include <signal.h>
#include "http_conn/http_conn.h"

#define MAX_FD 65535 //最大文件描述符个数
#define MAX_EVENT_NUM 10000 //最大监听事件数量

//添加信号捕捉
void addsig(int sig, void(handler)(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

//添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);

//从epoll中删除文件描述符
extern void removefd(int epollfd, int fd);

//修改文件描述符
extern void modfd(int epollfd, int fd, int ev);


int main(int argc, char* argv[]) {

    if(argc <= 1) {
        printf("按照如下格式运行: %s post_number\n", basename(argv[0]));
        exit(-1);
    }

    //获取端口号
    int port = atoi(argv[1]);

    //对SIGPIE信号进行处理
    addsig(SIGPIPE, SIG_IGN);

    //创建线程池，初始化线程池
    threadpool<http_conn> * pool = NULL;
    try{
        pool = new threadpool<http_conn>;
    } catch(...) {
        exit(-1);
    }

    //创建数组用于保存所有的客户端信息
    http_conn * users = new http_conn[MAX_FD];

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    //设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(listenfd, (struct sockaddr*)&address, sizeof(address));

    //监听
    listen(listenfd, 5);

    //创建epoll对象，事件数组，添加
    epoll_event events[MAX_EVENT_NUM];
    int epollfd = epoll_create(5);

    //将监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    while(1) {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
        if((num < 0) && (errno != EINTR)) {
            printf("epoll failure\n");
            break;
        }
        //printf("aaa\n");
        //循环遍历事件数组
        for(int i = 0; i < num; i++) {
            //printf("num : %d\n", num);
            int sockfd = events[i].data.fd;
            //printf("bbb\n");
            if(sockfd == listenfd) {
                //有客户连接进来
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlen);
                //printf("ccc\n");
                if(http_conn::m_user_count >= MAX_FD) {
                    //目前连接数满了
                    //需要给客户端回复:服务器正忙
                    //printf("Already Full!\n");
                    close(connfd);
                    continue;
                }
                //将新的客户数据初始化,放到数组中
                users[connfd].init(connfd, client_address);
            } else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //对方异常断开或者错误等事件
                users[sockfd].close_conn();
            } else if(events[i].events & EPOLLIN) {
                printf("\nread now\n");
                if(users[sockfd].read()) {
                    //一次性把所有数据都读完
                    //printf("append\n");
                    pool -> append(users + sockfd);
                } else {
                    //printf("append\n");
                    users[sockfd].close_conn();
                }
            } else if(events[i].events & EPOLLOUT) {
                printf("write now\n");
                if(!users[sockfd].write()) {
                    users[sockfd].close_conn();
                }
            }
            //printf("other\n");
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;

    return 0;
}