#pragma once
#include "../base/noncopyable.h"
#include "../lock/MutexLock.h"
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <cstdint>

class Condition : noncopyable
{
 public:
    explicit Condition(MutexLock& mutex)
        : mutex_(mutex)
    {
        pthread_cond_init(&pcond_, NULL);
    }

    ~Condition()
    {
        pthread_cond_destroy(&pcond_);
    }

    void wait()
    {
        pthread_cond_wait(&pcond_, mutex_.get());
    }

    // returns true if time out, false otherwise.
    bool waitForSeconds(double seconds){
        struct timespec abstime;
        clock_gettime(CLOCK_REALTIME, &abstime);
        abstime.tv_sec += static_cast<time_t>(seconds);
        return ETIMEDOUT == pthread_cond_timedwait(&pcond_, mutex_.get(), &abstime);
  }

    void notify()
    {
        pthread_cond_signal(&pcond_);
    }

    void notifyAll()
    {
        pthread_cond_broadcast(&pcond_);
    }

private:
    MutexLock& mutex_;
    pthread_cond_t pcond_;
};