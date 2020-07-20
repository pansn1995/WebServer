#ifndef EVENTLOOP_H
#define EVENTLOOP_H
#include "../base/Thread.h"
#include "../base/noncopyable.h"
#include "../log/Logging.h"
#include "../base/CurrentThread.h"
#include "../epoll/Epoll.h"
#include "../epoll/Channel.h"
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <iostream>

class EventLoop :noncopyable{
public:
    typedef std::function<void()> Functor;
    typedef std::shared_ptr<Channel> SP_Channel;
    typedef std::vector<SP_Channel> ChannelList;

    EventLoop();
    ~EventLoop();

    //对于一个线程的事件循环,调用epoll的poll函数，返回事件集合，对每个事件调用回调函数
    void loop();
    //终止loop函数的事件循环
    void quit();
    //运行回调函数cb，在其他线程会将函数加入队列中
    void runInLoop(Functor&& cb);
    //添加函数到队列中，如果是其他函数，或者当前在调用等待的cb函数(doPendingFunctor函数)
    void queueInLoop(Functor&& cb);
    //判断是否是当前loop所在线程在调用
    bool isInLoopThread()  const{return threadId_==CurrentThread::tid();}

    void assertInLoopThread() { assert(isInLoopThread()); }

    void setOnTimeCallback(int intervals, Functor &&callback) ;

    void removeFromPoller( SP_Channel channel) {
        poller_->removeChannel(channel);
    }

    void updatePoller( SP_Channel channel) {
        poller_->updateChannel(channel);
    }

    void addToPoller( SP_Channel channel ) {
        poller_->addChannel(channel);
    }
private:
    bool looping_;
    ChannelList activeChannelList_;// 就绪事件链表
    std::shared_ptr<Epoll> poller_;
    int wakeupFd_;
    std::atomic<bool> quit_;
    mutable MutexLock mutex_;
    std::vector<Functor> pendingFunctors_;
    bool callingPendingFunctors_;
    const pid_t threadId_;
    SP_Channel pwakeupChannel_;

    void wakeUp();//向eventfd写入，从而唤醒epoll
    void handleRead();//读取eventfd中的数据，供给下一次写入
    void handleError();
    void handleOnTime();
    void doPendingFunctors();//执行等待的回调函数
   
    int timerFd_;// 定时触发文件描述符
    SP_Channel ponTimeChannel_;//对应的channel
    Functor onTimeCallback_;//定时器回调函数
};

#endif