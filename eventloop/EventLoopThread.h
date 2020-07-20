#ifndef EVENTLOOP_THREAD_H
#define EVENTLOOP_THREAD_H

#include <string>
#include <functional>
#include <unistd.h>
#include "../base/Thread.h"
#include "../lock/Condition.h"
#include "../lock/MutexLock.h"
#include "EventLoop.h"
#include "../base/noncopyable.h"

class EventLoopThread
{
public:
    EventLoopThread(std::string n = std::string());
    ~EventLoopThread();

    EventLoop* getLoop();
    pid_t getThreadId() const 
    { return tid_; }
    void setName(std::string n)
    {
        name_ = n;
        thread_.setName(name_);
    }
    void start();
    void threadFunc();
private:
    Thread thread_;
    EventLoop *loop_;
    pid_t tid_;
    std::string name_;//线程名
    MutexLock mutex_;
    Condition cond_;
};
#endif



