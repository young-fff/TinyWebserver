#include "http_conn.h"

int http_conn::m_epollfd = -1;  
int http_conn::m_user_count = 0;   

//设置文件描述符非阻塞
void setnonblocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
}

//向epoll中添加需要监听的文件描述符
void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP; 

    if(one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    //设置文件描述符非阻塞
    setnonblocking(fd);
}

//从epoll中移除监听的文件描述符
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//修改文件描述符,重置socket上的EPOLLONESHOT事件,以确保下一次可读时,EPOLLIN事件能被触发
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//初始化连接
void http_conn::init(int sockfd, const sockaddr_in & addr) {
    m_sockfd = sockfd;
    m_address = addr;

    //端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //添加到epoll对象中
    addfd(m_epollfd, sockfd, true);
    m_user_count++; //总用户数加1

    init();
}

void http_conn::init() {
    m_check_state = CHECK_STATE_REQUESTLINE;    //初始状态为解析请求首行
    m_checked_index = 0;
    m_start_line = 0;
    m_start_line = 0;
}

//关闭连接
void http_conn::close_conn() {
    if(m_sockfd != -1) {
        //printf("CLOSE NOW");
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--; //关闭一个连接，总用户数减1
    }
}

//循环读取客户数据，直到无数据可读或者对方关闭连接
bool http_conn::read() {
    if(m_read_index > READ_BUFFER_SIZE) {
        printf("超出读缓冲范围");
        return false;
    }

    //读取到的字节
    int bytes_read = 0;
    while(1) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_index, READ_BUFFER_SIZE - m_read_index, 0);
        if(bytes_read == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                //没有数据
                break;
            }
            return false;
        } else if(bytes_read == 0) {
            //对方关闭连接
            return false;
        }
        m_read_index += bytes_read;
    }
    printf("读取到了数据:\n %s\n", m_read_buf);
    return true;
}

//主状态机，解析请求
http_conn::HTTP_CODE http_conn::process_read() {

    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;

    char * text = 0;

    while((((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) 
    || (line_status = parse_line()) == LINE_OK)) {
        //解析到了一行完整的数据，或者解析到了请求体
    }

    return NO_REQUEST;
} 

http_conn::HTTP_CODE http_conn::parse_request_line(char * text) {
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_header(char * text) {
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char * text) {
    return NO_REQUEST;
}

http_conn::LINE_STATUS http_conn::parse_line() {
    return NO_REQUEST;
}

bool http_conn::write() {
    printf("一次性写完数据\n");
    return true;
}


//由线程池中的工作线程调用，是处理HTTP请求的入口函数
void http_conn::process() {
    //解析HTTP请求
    HTTP_CODE read_ret =  process_read();
    if(read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    printf("parse request, create response\n");

    //生成响应
}