#ifndef _FILEUTIL_H
#define _FILEUTIL_H

#include "../base/noncopyable.h"
#include <string>

//实现一个文件工具类，包含append和write功能 
class AppendFile: noncopyable {
public: 
    explicit AppendFile(std::string filename);
    ~AppendFile();
    void append(const char* logline, const size_t len);
    void flush();
private:
    size_t write(const char* logline, size_t len);
    FILE* fp_;
    char buffer_[1<<16];
};

#endif