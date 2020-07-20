#ifndef EVENTLOOP_THREAD_POOL_H
#define EVENTLOOP_THREAD_POOL_H

#include <vector>
#include "EventLoop.h"
#include "EventLoopThread.h"

class EventLoopThreadPool
{
public:
    EventLoopThreadPool(EventLoop *mainLoop, int threadNum = 0);
    ~EventLoopThreadPool();

    void start();
    EventLoop* getNextLoop();

private:
    EventLoop *mainLoop_;
    int threadNum_;
    bool started_;
    int index_;
    std::vector<std::shared_ptr<EventLoopThread>> threadList_;
};

#endif
