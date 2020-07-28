#include <string>
using std::string;
#include "EventLoopThreadPool.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *mainLoop, int threadNum)
    : mainLoop_(mainLoop),
      threadNum_(threadNum),
      started_(false),
      index_(0)
{
    if (threadNum_ <= 0) {
        LOG << "threadNum_ <= 0";
        abort();
    }
}

EventLoopThreadPool::~EventLoopThreadPool()
{
   LOG << "~EventLoopThreadPool()";
}

void EventLoopThreadPool::start()
{
    mainLoop_->assertInLoopThread();
    started_=true;
    for (int i = 0; i < threadNum_; ++i) {
        std::shared_ptr<EventLoopThread> t=std::make_shared<EventLoopThread>();
        t->setName("IO thread"+std::to_string(i+1));
        threadList_.push_back(t);
        t->start();
    }
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
    mainLoop_->assertInLoopThread();
    assert(started_);
    if(threadNum_ > 0)
    {
        //RR策略
        EventLoop *loop = threadList_[index_]->getLoop();
        index_ = (index_ + 1) % threadNum_;
        return loop;
    }
    return mainLoop_;
}
