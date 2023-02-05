#ifndef CONN_POOL_H
#define CONN_POOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <jsoncpp/json/json.h>
#include "mysql_conn.h"


//单例模式
class conn_pool
{
public:
    // 得到单例对象
    static conn_pool* getConnectPool();
    // 删除拷贝构造函数
    conn_pool(const conn_pool& obj) = delete;
    // 删除拷贝赋值运算符重载函数
    conn_pool& operator=(const conn_pool& obj) = delete;

    // 从连接池中取出一个连接
    std::shared_ptr<mysql_conn> getConnection();

    //析构函数
    ~conn_pool();
private:
    // 构造函数私有化
    conn_pool();
    bool parse_json();
    void produce_conn();
    void recycle_conn();
    void add_conn();

    std::string m_ip;             // 数据库服务器ip地址
    std::string m_user;           // 数据库服务器用户名
    std::string m_dbName;         // 数据库服务器的数据库名
    std::string m_passwd;         // 数据库服务器密码
    unsigned short m_port;   // 数据库服务器绑定的端口
    int m_minSize;           // 连接池维护的最小连接数
    int m_maxSize;           // 连接池维护的最大连接数
    int m_max_freetime;       // 连接池中连接的最大空闲时长
    int m_timeout;           // 连接池获取连接的超时时长

    std::queue<mysql_conn*> m_conn_queue;
    std::mutex m_mutexQ;
    std::condition_variable m_cond;
};


#endif