#ifndef CHANNEL_H
#define CHANNEL_H

#include <functional>
#include "../log/Logging.h"

class Channel
{
public:
    typedef std::function<void()> Callback;
    typedef uint32_t event_t;

    Channel();
    ~Channel();

    void setFd(int fd) 
    {
        fd_ = fd; 
    }

    int getFd() const
    { 
        return fd_; 
    }

    void setEvents(event_t events)
    { 
        events_ = events; 
    }

    void setREvents(event_t events) 
    {
        revents_ = events;
    }

    event_t getEvents() const
    { 
        return events_; 
    }

    void handleEvent();

    // 设置Channel注册器上的回调函数
    void setReadHandle(Callback &&cb)
    {
        readHandler_ = cb; 
    }

    void setWriteHandle(Callback &&cb)
    {
        writeHandler_ = cb; 
    }    

    void setErrorHandle(Callback &&cb)
    { 
        errorHandler_ = cb;
    }

    void setCloseHandle(Callback &&cb)
    {
        closeHandler_ = cb;
    }
    
private:
    int fd_;
    event_t events_;// Channel希望注册到epoll的事件 
    event_t revents_;// epoll返回的激活事件

    //事件触发时执行的回调函数(事件处理器)
    Callback readHandler_;
    Callback writeHandler_;
    Callback errorHandler_;
    Callback closeHandler_;
};
#endif