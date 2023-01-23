#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <error.h>
#include "../locker/locker.h"
#include <sys/uio.h>
#include <errno.h>

class http_conn {
public:
    
    static int m_epollfd;   //所有的socket上的事件都被注册到同一个epoll
    static int m_user_count;    //统计用户的数量
    static const int READ_BUFFER_SIZE = 2048;   //读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 2048;   //写缓冲区大小

    //HTTP请求方法，只支持GET
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};
    
    /*
    解析客户端请求时，主状态机的状态
    CHECK_STATE_REQUESTLINE ： 当前正在分析请求行
    CHECK_STATE_HEADER ： 当前正在分析头部字段
    CHECK_STATE_CONTENT ： 当前正在解析请求体
    */
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};

    /*
    服务器处理HTTP请求的可能结果
    */


    http_conn(){}
    ~http_conn(){}

    //处理客户端请求
    void process();
    void init(int sockfd, const sockaddr_in & addr);
    void close_conn();  //关闭连接
    bool read();    //非阻塞的读
    bool write();   //非阻塞的写

private:
   
    int m_sockfd;  // 该HTTP连接的socket
    sockaddr_in m_address;  //通信的socket地址
    char m_read_buf[READ_BUFFER_SIZE];   //读缓冲区
    int m_read_index;   //读缓冲区中以及读入的客户端数据的最后一个字节的下一个位置 
};

#endif