#ifndef CONFIG_H
#define CONFIG_H

#include <getopt.h>
#include <string>

using namespace std;

class Config{
public:
    Config();
    ~Config(){};

    void parse_arg(int argc, char*argv[]);

    //端口号
    int PORT;

    //日志的地址
    string LOG_ADDRESS;

    //线程池内的线程数量
    int THREAD_NUM;

    //连接的不活跃时间
    int UNACTIVE_TIME;

};
#endif