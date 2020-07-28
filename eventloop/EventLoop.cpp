#include "EventLoop.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <iostream>

__thread EventLoop* t_loopInThisThread = 0;

int createEventfd() {
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG << "Failed in eventfd";
        abort();
    }
    return evtfd;
}

int createTimerFd(int intervals) 
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC|TFD_NONBLOCK);
    if (timerfd <= 0)
    {
        LOG << "Failed in create timerfd" ;
    }
    
    struct itimerspec its;
    its.it_value.tv_sec = 2;    // 第一次在2s后触发可读
    its.it_value.tv_nsec = 0;//500000000;
    its.it_interval.tv_sec = 1; // interval
    its.it_interval.tv_nsec =0; //1000000*intervals;
    if (::timerfd_settime(timerfd, 0, &its, nullptr) != 0) 
    {
        LOG << "Fail to set timerfd!" ;
    }
    return timerfd;
}

EventLoop::EventLoop()
    : looping_(false),
      activeChannelList_(),
      poller_(std::make_shared<Epoll>()),
      wakeupFd_(createEventfd()),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      pwakeupChannel_(std::make_shared<Channel>()) ,
      ponTimeChannel_(std::make_shared<Channel>())
{
    if(t_loopInThisThread)
    {
        LOG<<"Anoter EventLoop exists in this thread "<<std::to_string(threadId_);
    }
    else{
        t_loopInThisThread=this;
    }
    pwakeupChannel_->setFd(wakeupFd_);
    pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
    pwakeupChannel_->setReadHandle(std::bind(&EventLoop::handleRead, this));
    pwakeupChannel_->setErrorHandle(std::bind(&EventLoop::handleError, this));
    addToPoller(pwakeupChannel_);
}

EventLoop::~EventLoop()
{
    close(wakeupFd_);
    t_loopInThisThread=nullptr;
}

void EventLoop::wakeUp() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, (char*)(&one), sizeof one);
    if (n != sizeof one) {
        LOG << "EventLoop::wakeup() writes " << std::to_string(n) << " bytes instead of 8";
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG << "EventLoop::handleRead() reads " << std::to_string(n) << " bytes instead of 8";
    }
}

void EventLoop::handleError() {
    ;
}

void EventLoop::handleOnTime()
{
    uint64_t tmp;
    ssize_t n = read(timerFd_, &tmp, sizeof(uint64_t));
    if (n != sizeof tmp) {
        LOG << "EventLoop::handleOnTime() read " << std::to_string(n) << " bytes instead of 8";
    }
    onTimeCallback_();
}

void EventLoop::setOnTimeCallback(int intervals, Functor &&callback) 
{
    onTimeCallback_ = callback;
    timerFd_ = createTimerFd(intervals);
    ponTimeChannel_->setFd(timerFd_);
    ponTimeChannel_->setEvents(EPOLLIN | EPOLLET);
    ponTimeChannel_->setReadHandle(std::bind(&EventLoop::handleOnTime, this));
    addToPoller(ponTimeChannel_); 
}

void EventLoop::loop(){
    assert(!looping_);
    assert(isInLoopThread());
    looping_=true;
    quit_ = false;

    while (!quit_) {
        activeChannelList_.clear();
        poller_->epoll(activeChannelList_);
        for (auto& it : activeChannelList_) it->handleEvent();
        doPendingFunctors();
    }
    looping_ = false;
}

void EventLoop::runInLoop(Functor&& cb) {
    if (isInLoopThread())
        cb();
    else
        queueInLoop(std::move(cb));
}

void EventLoop::queueInLoop(Functor&& cb) {

    {
        MutexLockGuard lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_) wakeUp();
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (size_t i = 0; i < functors.size(); ++i) functors[i]();

    callingPendingFunctors_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeUp();
    }
}