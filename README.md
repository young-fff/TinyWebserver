# TinyWebserver
A tiny webserver for HTTP

## 开发环境
- 操作系统：Ubuntu 22.04
- 编辑器：Vscode
- 编译器：g++ 11.3.0
- 压测工具：[WebBench](https://github.com/young-fff/TinyWebserver/tree/main/webbench-1.5)

## 技术点
* 使用了epoll边沿触发IO多路复用技术
* 使用了一个固定线程数的线程池
* 使用了互斥锁及条件变量用于线程控制
* 使用了主从状态机解析HTTP请求
* 使用了双向升序链表定时器剔除非活动连接客户
* 使用单例模式实现mysql连接池
* 内置基于跳表的kv存储引擎


## 压测结果
* 并发连接总数：10000
* 访问服务器时间：5s
* 所有访问均成功

## 运行方式
* git clone 到本地，修改http_conn.cpp中的网站根目录
```
//网站根目录
const char* doc_root = "/home/youngff/program/websever/resources";

修改为本地resources地址
```
* 修改完成后在项目的根目录执行
```
make
./main 10000   #此处的10000为端口号，可改为任意可用值
```
* 运行成功后即可进入浏览器通过以下网址访问浏览器静态资源
```
http://(本地Ip地址):10000/index.html
```

------
* 如果想要运行没有定时器的版本,需要修改Makefile重新编译运行

-------

* 如果想要测试mysql连接池部分，测试前确认已安装MySQL数据库

    ```C++
    // 建立yourdb库
    create database testdb;

    // 创建user表
    USE testdb;
    CREATE TABLE user(
        username char(50) NULL,
        age int(10) NULL
    )ENGINE=InnoDB;

    // 添加数据
    INSERT INTO user(username, passwd) VALUES('xxs', '20');
    ```

## 代码框架

```
.
├── http_conn           http连接部分
│   ├── http_conn.h     http_conn类
│   └── http_conn.cpp   http_conn类实现
|
├── locker          锁部分
│   └── locker.h        locker类实现
|
├── resources      静态资源
│   ├── index.html
│   ├── test.html
│   ├── img
│   ├── js
│   └── css
├── mysql_conn     数据库部分
│   ├── conn_pool.cpp   conn_pool类实现
│   ├── conn_pool.h     conn_pool类
│   ├── mysql_conn.cpp  mysql_conn类实现
│   ├── mysql_conn.h    mysql_conn类
│   ├── dbconfig.json   配置文件
│   ├── main.cpp        测试程序
│   └── Makefile        
|
├── threadpool      线程池部分
│   └── threadpool.h    thread_pool类实现
|
├── webbench-1.5   压力测试
|
├── timer          定时器部分
│   └── lst_timer.h     util_timer类,sort_timer_lst 类实现
|
├── Makefile
|
├── main_notimer.cpp   无定时器版本
|
├── main.cpp        定时器版本
|
└── README.md       
```