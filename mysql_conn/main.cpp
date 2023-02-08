#include <iostream>
#include <memory>
#include <thread>
#include "mysql_conn.h"
#include "conn_pool.h"


using namespace std;
//1.单线程：使用/不使用连接池
//2.多线程：使用/不使用连接池

void op1(int begin, int end) {
    for(int i = begin; i < end; i++) {
        mysql_conn conn;
        bool ret1 = conn.connnect("xxs", "password", "testdb", "127.0.0.1", 3306);
        char sql[1024] = {0};
        sprintf(sql, "insert into user values('xxs', %d);", i);
        conn.update(sql);
    }
}


void op2(conn_pool* pool, int begin, int end) {
    for(int i = begin; i < end; i++) {
        shared_ptr<mysql_conn> conn = pool -> getConnection();
        char sql[1024] = {0};
        sprintf(sql, "insert into user values('xxs', %d);", i);
        conn -> update(sql);
    }
}
//非连接池,单线程,500次用时5745 ms
//非连接池,单线程,5000次用时68721ms
void test1() {
    steady_clock::time_point begin = steady_clock::now();
    op1(0, 5000);
    steady_clock::time_point end = steady_clock::now();
    auto length = end - begin;
    cout << "非连接池,单线程,用时" << length.count() / 1000000 << " ms" << endl;
}

//连接池,单线程,500次用时1843 ms
//连接池,单线程,5000次用时18287 ms
void test2() {
    conn_pool* pool = conn_pool::getConnectPool();
    steady_clock::time_point begin = steady_clock::now();
    op2(pool, 0, 5000);
    steady_clock::time_point end = steady_clock::now();
    auto length = end - begin;
    cout << "连接池,单线程,用时" << length.count() / 1000000 << " ms" << endl;
}

//非连接池,多线程,5000次用时46967 ms
void test3() {
    steady_clock::time_point begin = steady_clock::now();
    thread t1(op1, 0, 1000);
    thread t2(op1, 1000, 2000);
    thread t3(op1, 2000, 3000);
    thread t4(op1, 3000, 4000);
    thread t5(op1, 4000, 5000);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    steady_clock::time_point end = steady_clock::now();
    auto length = end - begin;
    cout << "非连接池,多线程,用时" << length.count() / 1000000 << " ms" << endl;
}

//连接池,多线程,5000次用时5175 ms
void test4() {
    conn_pool* pool = conn_pool::getConnectPool();
    steady_clock::time_point begin = steady_clock::now();
    thread t1(op2, pool, 0, 1000);
    thread t2(op2, pool, 1000, 2000);
    thread t3(op2, pool, 2000, 3000);
    thread t4(op2, pool, 3000, 4000);
    thread t5(op2, pool, 4000, 5000);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    steady_clock::time_point end = steady_clock::now();
    auto length = end - begin;
    cout << "连接池,多线程,用时" << length.count() / 1000000 << " ms" << endl;
}

int query() {
    mysql_conn conn;
    bool ret1 = conn.connnect("xxs", "password", "testdb", "127.0.0.1", 3306);
    string sql = "insert into user values('xxs', 10);";
    bool ret2 = conn.update(sql);
    if(ret1) {
        cout << "success!" << endl;
    } else {
        cout << "fail!" << endl;
    }

    sql = "select * from user;";
    conn.query(sql);
    while(conn.next()) {
        cout << conn.value(0) << ", " << conn.value(1) << endl;
    }
    return 0;
}

int main() {
    //query();
    test4();
    return 0;
}