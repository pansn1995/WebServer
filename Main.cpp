#include "config/Config.h"
#include <signal.h>
#include "eventloop/EventLoop.h"
#include "http/HttpServer.h"
#include "log/Logging.h"
#include <iostream>

int main(int argc, char *argv[]){

    signal(SIGPIPE, SIG_IGN);
    Config config;
    config.parse_arg(argc, argv);
    Logger::setLogFileName(config.LOG_ADDRESS);
    
    EventLoop mainloop;
    HttpServer myHttpServer(&mainloop,config.PORT,config.THREAD_NUM,config.UNACTIVE_TIME);
    myHttpServer.start();
    mainloop.loop();
    return 0;
}