#include "Thread.h"
#include "CurrentThread.h" 
#include <memory>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <stdint.h>
#include <assert.h>

namespace CurrentThread {
    __thread int         t_cacheTid = 0;
    __thread char        t_tidString[32];
    __thread int         t_tidStringLength = 6; // tid length = 5 
    __thread const char* t_threadName = "default";
}

// get thread tid use syscall
pid_t gettid() {
    return static_cast<pid_t>(::syscall(SYS_gettid));
}

void CurrentThread::cacheTid() {
    if(t_cacheTid == 0) {
        t_cacheTid = gettid();
        t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d", t_cacheTid);
    }
}

//封装了线程的数据信息
struct ThreadData {
    typedef Thread::ThreadFunc ThreadFunc;
    ThreadFunc      func_;//函数
    std::string          name_;//线程名
    pid_t*          tid_;//线程id
    CountDownLatch* latch_;//倒计时

    ThreadData(const ThreadFunc& func, const std::string& name, pid_t *tid, CountDownLatch* latch):
    func_(func),
    name_(name),
    tid_(tid),
    latch_(latch) {}

    void runInThread() {
        *tid_ = CurrentThread::tid();
        tid_ = nullptr;//防止之后修改
        latch_->countDown();//减一到0通知主线程，已经要开始执行
        latch_ = nullptr;//防止之后修改

        CurrentThread::t_threadName = name_.empty() ? "Thread" : name_.c_str();
        prctl(PR_SET_NAME, CurrentThread::t_threadName);
        
        func_();
        CurrentThread::t_threadName = "finished";
    }
};

void *startThread(void *obj) {
    ThreadData* data = static_cast<ThreadData*>(obj);
    data->runInThread();
    delete data;
    return nullptr;
}

Thread::Thread(const ThreadFunc& func, const std::string& name)
    : started_(false),
      joined_(false),
      pthreadId_(0),
      tid_(0),
      func_(func),
      name_(name),
      latch_(1)//主线程只等待一个线程
{
    setDefaultName();
}

Thread::~Thread() {
    if(started_ && !joined_) 
        pthread_detach(pthreadId_);
}

void Thread::setDefaultName() {
    if(name_.empty()) {
        char buf[32];
        snprintf(buf, sizeof buf, "Thread");
        name_ = buf;
    }
}

void Thread::start() {
    assert(!started_);
    started_ = true;
    ThreadData* data = new ThreadData(func_, name_, &tid_, &latch_);
    if(pthread_create(&pthreadId_, nullptr, &startThread, data)) {
        //创建线程失败
        started_ = false;
        delete data;
    } else {
        //创建成功，当前线程等待子线程开始执行
        latch_.wait();
        assert(tid_ > 0);
    }
}

int Thread::join() {
    assert(started_);
    assert(!joined_);
    joined_ = true;
    return pthread_join(pthreadId_, nullptr);
}
