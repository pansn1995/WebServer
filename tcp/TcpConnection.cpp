#include <cstdio>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include "TcpConnection.h"
#include "../eventloop/EventLoop.h"
#include "../epoll/Channel.h"
#include <memory>

TcpConnection::TcpConnection(EventLoop *loop, int fd, struct sockaddr_in clientaddr)
    : loop_(loop),
      spChannel_(std::make_shared<Channel>()),
	  socket_(fd),
      clientaddr_(clientaddr),
	  halfClose_(false),
	  connected_(true),
      active_(false),
      httpClosed_(false)
{
    spChannel_->setFd(fd);
    spChannel_->setEvents(EPOLLIN | EPOLLET);
    spChannel_->setReadHandle(std::bind(&TcpConnection::handleRead, this));
    spChannel_->setWriteHandle(std::bind(&TcpConnection::handleWrite, this));
    spChannel_->setCloseHandle(std::bind(&TcpConnection::handleClose, this));
    spChannel_->setErrorHandle(std::bind(&TcpConnection::handleError, this));    
}

TcpConnection::~TcpConnection()
{}

void TcpConnection::addChannelToLoop()
{
    // 主线程通过任务队列通知IO线程注册TcpConnection连接
	loop_->queueInLoop(std::bind(&EventLoop::addToPoller, loop_, spChannel_));
}

//void TcpConnection::setChannelOut(){
    //    event_t events = spChannel_->getEvents();
    //    spChannel_->setEvents(events | EPOLLOUT);
	//	loop_->updatePoller(spChannel_);
  //  }

void TcpConnection::handleWrite()
{
    if (!connected_) return;
    int result = sendn(socket_.fd(), bufferOut_);
    if(result >= 0)
    {
		event_t events = spChannel_->getEvents();
        if(bufferOut_.size() > 0)
        {
			//缓冲区满了，数据没发完，就设置EPOLLOUT事件触发			
			spChannel_->setEvents(events | EPOLLOUT);
			loop_->updatePoller(spChannel_);
        }else{
			//数据已发完
			spChannel_->setEvents(events & (~EPOLLOUT));
            loop_->updatePoller(spChannel_);
			sendCompleteCallback_();
            // 若Http层关闭，则可关闭连接
            if (httpClosed_)
                handleClose();
		}
    }else{        
		handleError();
    }    
}

void TcpConnection::handleRead()
{
    
    if (!connected_) return;
    // 接收数据，写入bufferIn_
    int result = recvn(socket_.fd(), bufferIn_);
    // 如果有读入数据，则将读入的数据提交给Http层处理
    if(result >= 0)
    {
        handleMessageCallback_(bufferIn_);
    }else
    {
        handleError();
    }
}

void TcpConnection::handleError()
{
	if(!connected_) 
        return; 
    forceClose();
}

void TcpConnection::handleClose()
{
	if (!connected_) 
        return; 
    if (bufferOut_.empty()) 
    {
	    connected_ = false;
        active_ = false;
        loop_->removeFromPoller(spChannel_);// 从epoller中清除注册信息
	    closeCallback_();// 上层处理连接关闭工作
	    connectionCleanUp_();
    } else
    {   // 发送剩余数据
        handleWrite();    
    }
}

void TcpConnection::forceClose()
{
    if (loop_->isInLoopThread()) 
    {
        if (!connected_) { return; }
        connected_ = false;
        loop_->removeFromPoller(spChannel_);
        closeCallback_();
        httpClosed_ = true;
        connectionCleanUp_();
    } else
    {
        loop_->queueInLoop(std::bind(&TcpConnection::forceClose, shared_from_this()));
    }
}

void TcpConnection::checkWhetherActive()
{
    if (loop_->isInLoopThread()) 
    {
        // 当前连接处于活跃状态
        if (active_) 
        {
            isActiveCallback_(shared_from_this());// 更新时间轮信息
            active_ = false;
        }else{
            // 否则，强制关闭连接
            forceClose();
        }
    }else
    {
        loop_->queueInLoop(std::bind(&TcpConnection::checkWhetherActive, shared_from_this()));
    }
}

int TcpConnection::recvn(int fd, std::string &bufferIn)
{
    active_ = true;
    ssize_t nread = 0;
    ssize_t readSum = 0;
    while(true)
    {
        char buffer[BUFSIZE];// 每次最多只读BUFSIZE个字节
    	if ((nread = read(fd, buffer, BUFSIZE)) < 0)
		{
            if (errno == EAGAIN)//系统缓冲区未有数据，非阻塞返回
			{
				return readSum;
			}else if (errno == EINTR)// 被中断则重读
			{
				LOG<< "errno == EINTR" ;
				continue;
			}else{
				LOG<<"recv error";
				return -1;
			}
		}else if (nread == 0)// 返回0，客户端关闭socket，FIN
		{
            halfClose_ = true;
            halfCloseCallback_();
			return readSum;
		}
        bufferIn+= std::string(buffer, buffer + nread);
        readSum += nread;
    }
    return readSum;
}

int TcpConnection::sendn(int fd, std::string &bufferOut)
{
    active_ = true;
	ssize_t nbyte = 0;
	size_t length = 0;
	length = bufferOut.size();
    //nbyte = send(fd, buffer, length, 0);
	//nbyte = send(fd, bufferout.c_str(), length, 0);
    for (; ;) 
    {
	    nbyte = write(fd, bufferOut.c_str(), length);
	    if (nbyte > 0)
    	{
		    bufferOut.erase(0, nbyte);
	        return nbyte;
	    } 
        else if (nbyte < 0)//异常
	    {
		    if (errno == EAGAIN)//系统缓冲区满，非阻塞返回
		    {
			    LOG << "write errno == EAGAIN,not finish!" ;
			    return 0;
		    }
		    else if (errno == EINTR)
		    {
			    std::cout << "write errno == EINTR" << std::endl;
                continue;
		    }
		    else if (errno == EPIPE)
		    {
			    // 客户端已经close，并发了RST，继续write会报EPIPE
			    perror("write error");
			    LOG<< "write errno == client send RST" ;
			    return -1;
		    }
		    else
		    {
			    perror("write error");
			    LOG << "write error, unknow error" ;
			    return -1;
		    }
	    }
	    else//返回0
	    {
		    return 0;
	    }
    }
}
