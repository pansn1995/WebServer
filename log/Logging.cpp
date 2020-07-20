#include "Logging.h"
#include "../base/CurrentThread.h"
#include "../base/Thread.h"
#include "AsyncLogging.h"
#include <assert.h>
#include <iostream>
#include <time.h>
#include <sys/time.h>

static pthread_once_t once_control_ = PTHREAD_ONCE_INIT;
static AsyncLogging *AsyncLogger_;

std::string Logger::logFileName_ = "./WebServer.log";

void once_init() {
    AsyncLogger_ = new AsyncLogging(Logger::getLogFileName());
    AsyncLogger_->start();
}

void output(const char *msg, int len) {
    pthread_once(&once_control_, once_init);//后端进程开启
    AsyncLogger_->append(msg, len);//前端向缓冲区加入
}

Logger::Impl::Impl(const char *fileName, int line)
: stream_(),
line_(line),
basename_(fileName)
{
    formatTime();//加入时间
}

void Logger::Impl::formatTime() {
    struct timeval tv;
    time_t time;
    char str_t[26] = {0};
    gettimeofday(&tv, nullptr);
    time = tv.tv_sec;
    struct tm *p_time = localtime(&time);
    strftime(str_t, 26, "%Y-%m-%d %H:%M:%S ", p_time);
    stream_ << str_t;
}

Logger::Logger(const char *fileName, int line) 
: impl_(fileName, line) {
}

Logger::~Logger(){
    impl_.stream_ << " -- "<< impl_.basename_ << ":" << impl_.line_ << "\n";//发送log的文件名和行数
    const LogStream::Buffer& buf(stream().buffer());
    output(buf.data(), buf.length());//析构时把数据加到缓冲区
}
