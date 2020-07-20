#include "Config.h"

Config::Config(){
    //端口号,默认9006
    PORT = 9006;

    //日志写入地址
    LOG_ADDRESS = "./WebServer.log";
    
    //线程池内的线程数量,默认4
    THREAD_NUM = 4;

    //最大超时时间,默认不设置
    UNACTIVE_TIME=0;
}

void Config::parse_arg(int argc, char*argv[]){
    int opt;
    const char *str = "l:t:p:a:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
            case 'p':
            {
                PORT = atoi(optarg);
                break;
            }
            case 'l':
            {
                LOG_ADDRESS =optarg;
                if (LOG_ADDRESS.size() < 2 || optarg[0] != '/') {
                    printf("logPath should start with \"/\"\n");
                    abort();
                }
                break;
            }
            case 't':
            {
                THREAD_NUM = atoi(optarg);
                break;
            }
            case 'a':
            {
                UNACTIVE_TIME = atoi(optarg);
                break;
            }
            default:
            {
                break;
            }
        }
    }
}