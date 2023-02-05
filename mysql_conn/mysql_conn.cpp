#include <string>
#include "mysql_conn.h"

mysql_conn::mysql_conn() {
    m_sqlconn = mysql_init(nullptr);
    mysql_set_character_set(m_sqlconn, "utf8");
}

mysql_conn::~mysql_conn() {
    if(m_sqlconn) {
        mysql_close(m_sqlconn);
    }
    freeResult();
}

bool mysql_conn::connnect(std::string user, std::string passwd, std::string dbName, std::string ip, unsigned short port) {
    MYSQL* ptr = mysql_real_connect(m_sqlconn, ip.c_str(), user.c_str(), passwd.c_str(), dbName.c_str(), port, nullptr, 0);
    if(!ptr) {
        fprintf(stderr, "Failed to connect to database: Error: %s\n",mysql_error(m_sqlconn));
    }
    return ptr != nullptr;
}

bool mysql_conn::update(std::string sql) {
    int ret = mysql_query(m_sqlconn, sql.c_str());
    return ret == 0;
}

bool mysql_conn::query(std::string sql) {
    freeResult();//清空上次结果申请的内存
    int ret = mysql_query(m_sqlconn, sql.c_str());
    if(ret) {
        return false;
    } 
    m_result = mysql_store_result(m_sqlconn);
    return true;
}

bool mysql_conn::next() {
    if(m_result) {
        m_row = mysql_fetch_row(m_result);
        if(m_row) return true;
    }
    return false;
}

std::string mysql_conn::value(int index) {
    int col_count = mysql_num_fields(m_result);
    if(index >= col_count || index < 0) {
        return std::string();
    } 
    char* val = m_row[index];
    unsigned long length = mysql_fetch_lengths(m_result)[index];    //由于mysql_fetch_length返回对应每条数据的长度数组，只需取第index条即可
    return std::string(val, length);     //由于m_row为char**在val中可能有'\0'在中间，直接转化为string会只转化'\0'前面的部分,传入length参数后解决
}

bool mysql_conn::transaction() {
    return mysql_autocommit(m_sqlconn, false);
}

bool mysql_conn::commit() {
    return mysql_commit(m_sqlconn);
}

bool mysql_conn::rollback() {
    return mysql_rollback(m_sqlconn);
}

void mysql_conn::refresh_freetime() {
    //获取当前时间
    m_freetime = steady_clock::now();
}

long long mysql_conn::get_freetime() {
    //获取当前时间
    nanoseconds res = std::chrono::steady_clock::now() - m_freetime;
    //转换为低精度
    milliseconds millsec = duration_cast<milliseconds>(res);
    return millsec.count();
}

void mysql_conn::freeResult() {
    if(m_result) {
        mysql_free_result(m_result);
        m_result = nullptr;
    } 
}



