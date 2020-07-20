
#include <iostream>
#include <sys/epoll.h>
#include "Channel.h"

Channel::Channel() 
{}

Channel::~Channel()
{
    LOG << "free Channel" ;   
}

void Channel::handleEvent()
{
    //文件描述符读写关闭
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        LOG<< "Event EPOLLRDHUP" ;
        if (closeHandler_)
            closeHandler_();
    }
    //文件描述符可读或者是带外数据或者对端关闭
    if(revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
    {
        readHandler_();
    }
    //文件描述符可写
    if(revents_ & EPOLLOUT)
    {
        writeHandler_();
    }
    //文件描述符读写异常
    if (revents_ & EPOLLERR)
    {
        if (errorHandler_)
            errorHandler_();
    }
}
