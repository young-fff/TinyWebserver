#include <jsoncpp/json/json.h>
#include <fstream>
#include <thread>
#include "conn_pool.h"


using namespace Json;

conn_pool* conn_pool::getConnectPool() {
    //静态局部变量，访问范围为该函数内，但生命周期和程序相同
    //第一次调用时创建对象，之后的调用由于对象已经存在，所以调用得到的是和第一次调用同样的内存地址
    static conn_pool pool;  
    //返回静态局部对象的地址，得到类的唯一的实例
    return &pool;
}

bool conn_pool::parse_json() {
    std::ifstream ifs("dbconfig.json");
    Reader rd;
    Value root;
    rd.parse(ifs, root);
    if(root.isObject()) {
        m_ip = root["ip"].asString();
        m_user = root["user"].asString();
        m_dbName = root["dbName"].asString();
        m_passwd = root["passwd"].asString();
        m_port = root["port"].asInt();
        m_minSize = root["minSize"].asInt();
        m_maxSize = root["maxSize"].asInt();
        m_max_freetime = root["max_free_time"].asInt();
        m_timeout = root["timeout"].asInt();
        return true;
    }
    return false;
}

conn_pool::~conn_pool() {
    while(!m_conn_queue.empty()) {
        mysql_conn* conn = m_conn_queue.front();
        m_conn_queue.pop();
        delete conn;
    }
}

conn_pool::conn_pool() {
    //加载配置文件
    if(!parse_json()) {
        return;
    }
    //先创建最小数量的连接
    for(int i = 0; i < m_minSize; i++) {
        add_conn();
    }
    //不能调用join方法影响主线程
    std::thread producer(&conn_pool::produce_conn, this);
    std::thread recycler(&conn_pool::recycle_conn, this);
    //线程分离
    producer.detach();
    recycler.detach();
}


//生产数据库连接
//由于需要一直生产，而当连接池满了需要阻塞
void conn_pool::produce_conn() {
    while(1) {
        std::unique_lock<std::mutex> locker(m_mutexQ);
        //使用while是因为生产者被阻塞在m_cond.wait(locker)处
        //如果使用if，当被唤醒时可能多个生产者继续向下执行,生产超量的连接
        //而用while会让生产者被唤醒时在经过一次while条件判断，避免上述情况
        while(m_conn_queue.size() >= m_minSize ) {   //阻塞条件
            m_cond.wait(locker);
        }
        add_conn();
        //唤醒消费者，其实也唤醒生产者，但生产者会继续判断并阻塞
        //但事实上此程序中只有一个生产者线程且执行到了此行
        //所以其实是唤醒了所有消费者线程
        m_cond.notify_all();
    }
}

void conn_pool::add_conn() {
    mysql_conn* conn = new mysql_conn;
    conn -> connnect(m_user, m_passwd, m_dbName, m_ip, m_port);
    conn -> refresh_freetime();
    m_conn_queue.push(conn);
}

//回收数据库连接
void conn_pool::recycle_conn() {
    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::lock_guard<std::mutex> locker(m_mutexQ);
        while(m_conn_queue.size() > m_minSize) {
            mysql_conn* conn = m_conn_queue.front();
            if(conn -> get_freetime() >= m_max_freetime) {
                m_conn_queue.pop();
                delete conn;
            } else {
                break;
            }
        }
    }
}

std::shared_ptr<mysql_conn> conn_pool::getConnection() {
    std::unique_lock<std::mutex> locker(m_mutexQ);
    while(m_conn_queue.empty()) {
        //wait_for只阻塞指定时间
        //当指定时间用完而线程仍未唤醒，将返回状态timeout
        if(std::cv_status::timeout == m_cond.wait_for(locker, std::chrono::milliseconds(m_timeout))) {
            if(m_conn_queue.empty()) {
                //return nullptr;
                continue;
            }
        }
    }
    //指定shared_ptr连接的删除器，改为不释放资源，而将连接加入连接队列
    std::shared_ptr<mysql_conn> connptr(m_conn_queue.front(), [this](mysql_conn* conn) {
        std::lock_guard<std::mutex> locker(m_mutexQ);
        conn -> refresh_freetime();
        m_conn_queue.push(conn);
    });
    m_conn_queue.pop();
    //唤醒生产者，其实也唤醒消费者，但消费者会继续判断并阻塞
    m_cond.notify_all();
    return connptr;
}
