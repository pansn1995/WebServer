#include <iostream>
#include <cstdio> 
#include <cstdlib> 
#include <unistd.h> 
#include <errno.h>
#include <cstring>
#include "Epoll.h"

#define EVENTNUM 4096 

Epoll::Epoll()
    : epollfd_(-1),
      eventList_(EVENTNUM),
      channelMap_()
{
    epollfd_ = epoll_create(256);
    if(epollfd_ == -1)
    {
        perror("epoll_create");
        exit(1);
    }
    LOG << "epoll_create" << std::to_string(epollfd_) ;
}

Epoll::~Epoll()
{
    close(epollfd_);
}

void Epoll::epoll(ChannelList &activeChannelList)
{
    int nfds = epoll_wait(epollfd_, &*eventList_.begin(), static_cast<int>(eventList_.size()), -1);
    
    if(nfds == -1)
    {
        perror("epoll wait error");
    }
    for(int i = 0; i < nfds; ++i)
    {
        int events = eventList_[i].events;
        int fd = eventList_[i].data.fd;
        if(channelMap_.find(fd)!= channelMap_.end())
        {
            SP_Channel spChannel = channelMap_[fd];
            spChannel->setREvents(events);
            activeChannelList.push_back(spChannel);
        } else
        {            
            LOG<< "not find channel!" ;
        }
    }
}

void Epoll::addChannel(SP_Channel spChannel)
{
    int fd = spChannel->getFd();
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = spChannel->getEvents();
    ev.data.fd = fd;
    channelMap_[fd] = spChannel;

    if(epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        LOG<<"epoll add error";
    }
}

void Epoll::removeChannel(SP_Channel spChannel)
{
    int fd = spChannel->getFd();
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = spChannel->getEvents();
    ev.data.fd = fd;
    
    if(epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev) == -1)
    {
        LOG<<"epoll del error";
    }
    channelMap_.erase(fd);
}

void Epoll::updateChannel(SP_Channel spChannel)
{
    int fd = spChannel->getFd();
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = spChannel->getEvents();
    ev.data.fd = fd;

    if(epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &ev) == -1)
    {
        LOG<<"epoll update error";
        channelMap_.erase(fd);
    }
}