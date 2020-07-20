#pragma once
#include "noncopyable.h"
#include "../lock/MutexLock.h"
#include  "../lock/Condition.h"

//倒计时函数
//用于主线程等待多个子线程完成一些操作，或者，多个子线程等待主线程发送起跑

class CountDownLatch : noncopyable
{
public:

    explicit CountDownLatch(int count);//倒数几次

    void wait();//等待计数值变为0

    void countDown();//计数减一

    private:
    mutable MutexLock mutex_;
    Condition condition_ ;
    int count_;
};