#ifndef MYSQL_CONN_H
#define MYSQL_CONN_H

#include <string>
#include <chrono>
#include <mysql/mysql.h>

using namespace std::chrono;

class mysql_conn {
public:
    //初始化数据库连接
    mysql_conn();
    //释放数据库连接
    ~mysql_conn();
    //连接数据库
    bool connnect(std::string user, std::string passwd, std::string dbName, std::string ip, unsigned short port = 3306);
    //更新数据库
    bool update(std::string sql);
    //查询数据库
    bool query(std::string sql);
    //遍历查询得到的结果集
    bool next();
    //遍历结果集中的字段值
    std::string value(int index);
    //事务操作
    bool transaction();
    //提交事务
    bool commit();
    //事务回滚
    bool rollback();
    //刷新起始空闲时间点
    void refresh_freetime();
    //计算连接存活的总时长
    long long get_freetime();

private:
    void freeResult();

    MYSQL* m_sqlconn = nullptr;
    MYSQL_RES* m_result = nullptr;
    MYSQL_ROW  m_row = nullptr;     //MYSQL_ROW实际是字符串数组char**

    steady_clock::time_point m_freetime;
};

#endif