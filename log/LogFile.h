#ifndef _LOGFILE_H
#define _LOGFILE_H
#include "FileUtil.h"
#include "../base/noncopyable.h"
#include "../lock/MutexLock.h"
#include <memory>
#include <string>

// 归档
class LogFile : noncopyable {
public:
    LogFile(const std::string &basename, int flushEveryN = 1024);
    ~LogFile();

    void append(const char* logline, const size_t len);
    void flush();//手动刷新缓冲区
    //bool rollFile();
private:
    void append_unlocked(const char* logline, const size_t len);
    
    const std::string basename_;//日志文件名
    const int flushEveryN_;//固定刷新频率

    int count_;//当前写入次数
    std::unique_ptr<MutexLock> mutex_;//互斥锁
    std::unique_ptr<AppendFile> file_;//文件工具类——具体的写入
};

#endif
