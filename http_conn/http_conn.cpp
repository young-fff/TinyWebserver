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
    m_url = 0;
    m_version = 0;
    m_method = GET;
    m_linger = false;


    bzero(m_read_buf, READ_BUFFER_SIZE);
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
        text = get_line();

        m_start_line = m_checked_index;
        printf("got 1 http line : %s\n", text);

        switch(m_check_state) {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret = parse_header(text);
                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if(ret == GET_REQUEST) {
                    return do_request();
                }
                break;

            }   
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content(text);
                if(ret == GET_REQUEST) {
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
        return NO_REQUEST;
    }

    return NO_REQUEST;
} 

//解析HP请求行，获得请求方法，目标URL，HTTP版本
http_conn::HTTP_CODE http_conn::parse_request_line(char * text) {
    //GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t");

    //GET \0/index.html HTTP/1.1
    char * method = text;
    if(strcmp(method, "GET") == 0) {
        m_method = GET;
    } else {
        return BAD_REQUEST;
    }

    // /index.html THHP/1.1
    m_version = strpbrk(m_url, " \t");
    if(!m_version) {
        return BAD_REQUEST;
    }
    // /index.html\0HTTP/1.1
    *m_version++ = '\0';
    if(strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }
    //http://192.168.1.18:10000/index.html
    if(strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;     //192.168.1.18:10000/index.html
        m_url = strchr(m_url, '/');     //index.html
    }

    if(!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER;     //主状态机检查状态变成检查请求头

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_header(char * text) {
    //遇到空行，表示头部字段解析完毕
    if(text[0] == '\0') {
        //如果HTTP请求有消息体，则还需要读取m_content_length字节消息体
        //状态机转移到CHECK_STATE_CONTENT状态
        if(m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        //否则说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    } else if(strncasecmp(text, "Connection:", 11) == 0) {
        //处理Connection头部字段 Connection: keep-alive
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    } else if(strncasecmp(text, "Content-Length:", 15) == 0) {
        //处理Content-Length头部字段
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    } else if(strncasecmp(text, "Host:", 5) == 0) {
        //处理Host头部字段
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    } else {
        printf("opp! unknow header %s\n", text);
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char * text) {
    if(m_read_index >= (m_content_length + m_checked_index)) {
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

//解析一行，判断依据\r\n
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for( ; m_checked_index < m_read_index; ++m_checked_index) {
        temp = m_read_buf[m_checked_index];
        if(temp == '\r') {
            if(m_checked_index + 1 == m_read_index) {
                return LINE_OPEN;
            } else if(m_read_buf[m_checked_index + 1] == '\n') {
                m_read_buf[m_checked_index++] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if(temp == '\n') {
            if(m_checked_index > 1 && m_read_buf[m_checked_index - 1] == '\r') {
                m_read_buf[m_checked_index - 1] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        return LINE_OPEN;
    }


    return LINE_OK;
}

/*
当得到一个完整的/正确的HTTP请求时，我们就分析目标文件的属性
如果目标文件存在/对所有的客户可读，且不是目录，则使用mmap将其
映射到内存地址m_file_address处，并告诉调用者获取文件
*/
http_conn::HTTP_CODE http_conn::do_request() {
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len -1);
    //获取m_real_file文件的相关状态信息，-1失败，0成功
    if(stat(m_real_file, &m_file_stat) < 0) {
        return NO_RESOURCE;
    }
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
    bool write_ret = process_write(read_ret);
    if(!write_ret) {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd,EPOLLOUT);
}