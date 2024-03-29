#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <iostream>

#include "timer/lst_timer.h"
#include "http_conn/http_conn.h"
#include "locker/locker.h"
#include "threadpool/threadpool.h"

#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd = 0;

extern void addfd(int epollfd, int fd, bool one_shot);
extern void removefd(int epollfd, int fd);
extern void modfd(int epollfd, int fd, int ev);
extern int setnonblocking(int fd);
/*
int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

void addfd( int epollfd, int fd )
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
}
*/
void sig_handler( int sig )
{
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* )&msg, 1, 0 );//发送信号值进入管道
    errno = save_errno;
}

void addsig( int sig, void (handler)(int) )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset( &sa.sa_mask );  //暂时设为阻塞
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

void timer_handler()
{
    // 定时处理任务，实际上就是调用tick()函数
    timer_lst.tick();
    // 因为一次 alarm 调用只会引起一次SIGALARM 信号，所以我们要重新定时，以不断触发 SIGALARM信号。
    alarm(TIMESLOT);
}


/*void cb_func( client_data* user_data )
{
    epoll_ctl( epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0 );
    assert( user_data );
    close( user_data->sockfd );
    printf( "close fd %d\n", user_data->sockfd );
}
*/

// 定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之。
void cb_func( http_conn* user_data ) {
    epoll_ctl( epollfd, EPOLL_CTL_DEL, user_data->getfd(), 0 );
    assert( user_data );
    close( user_data->getfd() );
    printf( "close fd %d\n", user_data->getfd() );
}

int main( int argc, char* argv[] ) {
    if( argc <= 1 ) {
        printf( "usage: %s port_number\n", basename( argv[0] ) );
        exit(-1);
    }
    int port = atoi( argv[1] );

    addsig(SIGPIPE, SIG_IGN);  //对于终止信号，进行忽略。 防止客户端终止终止服务端
    
    //创建线程池，初始化线程池
    threadpool<http_conn> * pool = NULL; //线程池装的是工作线程，可以处理数据响应请求。
    try{
        pool = new threadpool<http_conn>;
    } catch(...) {
        exit(-1);
    }

    
    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( port );

    //创建监听套接字
    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );

    //设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //绑定
    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    //监听
    ret = listen( listenfd, 5 );
    assert( ret != -1 );

    //使用epoll多路复用
    //创建epoll对象
    epoll_event events[ MAX_EVENT_NUMBER ];
    int epollfd = epoll_create( 5 );
    assert( epollfd != -1 );

    //addfd( epollfd, listenfd );

    //将监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);    //允许多个线程同时监听文件描述符，不需要添加oneshot
    http_conn::m_epollfd = epollfd;

    // 创建管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert( ret != -1 );
    setnonblocking( pipefd[1] );

    addfd( epollfd, pipefd[0] ,true);//设置了单次命中

    //addfd( epollfd, pipefd[0] ,false);

    // 设置信号处理函数
    addsig( SIGALRM, sig_handler);
    addsig( SIGTERM, sig_handler);
    bool stop_server = false;

    //创建数组用于保存所有的客户端信息
    http_conn * users = new http_conn[FD_LIMIT];
    //client_data* users = new client_data[FD_LIMIT]; 
    bool timeout = false;
    alarm(TIMESLOT);  // 定时,5秒后产生SIGALARM信号

    while( !stop_server )
    {
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        if ( ( number < 0 ) && ( errno != EINTR ) ) {//产生信号会中断
            printf( "epoll failure\n" );
            break;//中断了
        }
    
        for ( int i = 0; i < number; i++ ) {
            int sockfd = events[i].data.fd;
            if( sockfd == listenfd ) {
                //有客户端连接进来
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                addfd( epollfd, connfd ,true);

                if(http_conn::m_user_count >= FD_LIMIT) {
                    //目前连接数满了
                    //需要给客户端回复:服务器正忙
                    //printf("Already Full!\n");
                    close(connfd);
                    continue;
                }
                //addfd( epollfd, connfd)
                
                //将新的客户数据初始化,放到数组中
                users[connfd].init(connfd, client_address);
                //users[connfd].address = client_address;
                //users[connfd].sockfd = connfd;
                
                // 创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表timer_lst中
                util_timer* timer = new util_timer;
                timer->user_data = &users[connfd];
                timer->cb_func = cb_func;
                time_t cur = time( NULL );
                timer->expire = cur + 3 * TIMESLOT;
                users[connfd].timer = timer;
                timer_lst.add_timer( timer );
            } else if( ( sockfd == pipefd[0] ) && ( events[i].events & EPOLLIN ) ) {
                // 有信号了，处理信号
                int sig;
                char signals[1024];
                ret = recv( pipefd[0], signals, sizeof( signals ), 0 );
                if( ret == -1 || ret == 0 ) {
                    continue;
                } else  {
                    for( int i = 0; i < ret; ++i ) {
                        switch( signals[i] )  {
                            case SIGALRM:
                            {
                                //延迟处理，因为IO优先级更高
                                // 用timeout变量标记有定时任务需要处理，但不立即处理定时任务
                                // 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务。
                                timeout = true;
                                break;
                            }
                            case SIGTERM:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
            } else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //对方异常断开或者错误等事件
                util_timer* timer = users[sockfd].timer; //为删除做准备
                if (timer){
                    timer_lst.del_timer(timer);
                }
                users[sockfd].close_conn();
            } else if(  events[i].events & EPOLLIN ) {   
                printf("\nread now\n");
                util_timer* timer = users[sockfd].timer;
                if(users[sockfd].read()) {
                    //一次性把所有数据都读完
                    //printf("append\n");
                    if( timer ) {
                        time_t cur = time( NULL );
                        timer->expire = cur + 3 * TIMESLOT;
                        printf( "adjust timer once\n" );
                        timer_lst.adjust_timer( timer );
                    }
                    pool -> append(users + sockfd);
                } else {
                    printf("----read error-----\n");
                    users[sockfd].close_conn();
                    if(timer) {
                        timer_lst.del_timer( timer );
                    }
                }
                /*
                memset( users[sockfd].buf, '\0', BUFFER_SIZE );
                ret = recv( sockfd, users[sockfd].buf, BUFFER_SIZE-1, 0 );
                printf( "get %d bytes of client data %s from %d\n", ret, users[sockfd].buf, sockfd );
                util_timer* timer = users[sockfd].timer;
                if( ret < 0 )
                {
                    // 如果发生读错误，则关闭连接，并移除其对应的定时器
                    if( errno != EAGAIN )
                    {
                        cb_func( &users[sockfd] );
                        if( timer )
                        {
                            timer_lst.del_timer( timer );
                        }
                    }
                }
                else if( ret == 0 )
                {
                    // 如果对方已经关闭连接，则我们也关闭连接，并移除对应的定时器。
                    cb_func( &users[sockfd] );
                    if( timer )
                    {
                        timer_lst.del_timer( timer );
                    }
                }
                else
                {
                    // 如果某个客户端上有数据可读，则我们要调整该连接对应的定时器，以延迟该连接被关闭的时间。
                    if( timer ) {
                        time_t cur = time( NULL );
                        timer->expire = cur + 3 * TIMESLOT;
                        printf( "adjust timer once\n" );
                        timer_lst.adjust_timer( timer );
                    }
                }
                */
            } else if(events[i].events & EPOLLOUT) {
                printf("write now\n");
                if(!users[sockfd].write()) {
                    users[sockfd].close_conn();
                    //timer_lst.del_timer( timer );
                }
            }
        }

        // 最后处理定时事件，因为I/O事件有更高的优先级。当然，这样做将导致定时任务不能精准的按照预定的时间执行。
        if( timeout ) {
            timer_handler();
            //std::cout << " -----time out-----" << std::endl;
            timeout = false;
        }
    }
    close(epollfd);
    close( listenfd );
    close( pipefd[1] );
    close( pipefd[0] );
    delete [] users;
    delete pool;
    return 0;
}
