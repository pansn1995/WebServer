#ifndef EPOLL_H
#define EPOLL_H

#include <vector>
#include <pthread.h>
#include <map>
#include <memory>
#include <sys/epoll.h>
#include "Channel.h"
#include "../base/noncopyable.h"

class Epoll:noncopyable
{
public:
    typedef std::shared_ptr<Channel> SP_Channel;
    typedef std::vector<SP_Channel> ChannelList;
    Epoll();
    ~Epoll();

    void epoll(ChannelList &activeChannelList);
    void addChannel(SP_Channel spChannel);
    void removeChannel(SP_Channel spChannel);
    void updateChannel(SP_Channel spChannel);
private:
    int epollfd_;
    std::vector<epoll_event> eventList_;
    std::map<int, SP_Channel> channelMap_;
};
#endif


