#ifndef CURRENTTHREAD_H
#define CURRENTTHREAD_H

#include <stdint.h>

//将gettid系统调用的结果保存在__thread 关键字修饰的变量里，表明是当前线程专属的独立实体。
//之后需要使用就不要再频繁调用gettid，可以从__thread中获取线程信息。

namespace CurrentThread {
    extern __thread int         t_cacheTid;//当前id
    extern __thread char        t_tidString[32];//当前tid的字符串表示，用于log
    extern __thread int         t_tidStringLength;//字符串长度
    extern __thread const char* t_threadName;//线程名字

    void cacheTid();

    inline int tid() {
        //当前tid很有可能为假时，进入，即当前可能是野生线程并且还没有缓存tid
        if(__builtin_expect(t_cacheTid == 0, 0)) {
            cacheTid();
        }
        return t_cacheTid;
    }

    inline const char* tidString() {
        return t_tidString;
    }

    inline int tidStringLength() {
        return t_tidStringLength;
    }

    inline const char* name() {
        return t_threadName;
    }
}

#endif