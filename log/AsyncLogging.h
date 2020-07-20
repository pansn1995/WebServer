#ifndef _ASYNCLOGGING_H
#define _ASYNCLOGGING_H
#include "../base/CountDownLatch.h"
#include "../lock/MutexLock.h"
#include "../base/Thread.h"
#include "LogStream.h"
#include "../base/noncopyable.h"
#include <functional>
#include <string>
#include <vector>

//异步日志
class AsyncLogging: noncopyable {
public:
    AsyncLogging(const std::string basename, int flushInterval = 2);
    ~AsyncLogging() {
        if(running_) {
            stop();
        }
    }
    //前端线程写入缓冲区
    void append(const char *logline, int len);

    void start() {
        running_ = true;
        thread_.start();
        latch_.wait();
    }

    void stop() {
        running_ = false;
        cond_.notify();
        thread_.join();
    }

private:
    //后端线程使用的函数，将buffers_x写入文件
    void threadFunc();

    typedef FixedBuffer<kLargeBuffer> Buffer;
    typedef std::vector<std::shared_ptr<Buffer>> BufferVector;
    typedef std::shared_ptr<Buffer> BufferPtr;
    const int flushInterval_;//等待条件变量触发的时间
    bool running_;//日志系统在运行吗？
    std::string basename_;//日志文件名
    Thread thread_;//后端io线程
    MutexLock mutex_;
    Condition cond_;
    BufferPtr currentBuffer_;//当前的缓冲
    BufferPtr nextBuffer_;//下一个缓冲
    BufferVector buffers_;//多个缓冲区组成的vector，已经写满了日志，待发送
    CountDownLatch latch_;

};

#endif
