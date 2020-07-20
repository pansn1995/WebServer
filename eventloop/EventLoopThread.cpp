#include <iostream>
#include <sstream>
#include <string>
#include "EventLoopThread.h"
#include "../base/CurrentThread.h"
EventLoopThread::EventLoopThread(std::string n)
    : thread_(std::bind(&EventLoopThread::threadFunc, this)),
      loop_(nullptr),
      name_(n),
      mutex_(),
      cond_(mutex_)
{}

EventLoopThread::~EventLoopThread()
{
    if(loop_ != nullptr) {
        loop_->quit();
        thread_.join();
        LOG<< "IO thread " << name_ << " finished" ;
    }
}

EventLoop* EventLoopThread::getLoop()
{
    return loop_;
}

void EventLoopThread::start()
{
    thread_.start();
    {
        MutexLockGuard lock(mutex_);
        while(loop_ == nullptr) {
            cond_.wait();
        }
    }
}

void EventLoopThread::threadFunc()
{
    EventLoop loop;
    {
        MutexLockGuard lock(mutex_);
        loop_ = &loop;
        cond_.notify();
    }

    loop.loop();
    MutexLockGuard lock(mutex_);
    loop_ = nullptr;
}