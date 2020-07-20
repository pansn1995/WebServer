#include <unistd.h>
#include <fcntl.h> 
#include <cstdlib>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "TcpServer.h"
#include "../epoll/Channel.h"
#include "../eventloop/EventLoop.h"
#include "TcpConnection.h"
using namespace std::placeholders;

void setNonblocking(int fd);

void setSocketNodelay(int fd);


TcpServer::TcpServer(EventLoop *loop, int port, int threadNum, int idleSeconds)
    : serverSocket_(),
      loop_(loop),
      eventLoopThreadPool_(new EventLoopThreadPool(loop, threadNum)),
      spServerChannel_(new Channel()),
      connCount_(0),
      timeWheel_(idleSeconds),
      removeIdleConnection_(idleSeconds > 0) 
{
    serverSocket_.setReuseAddr();// 设置地址复用  
    serverSocket_.bindAddress(port);// 绑定端口
    serverSocket_.listen();// 将套接字转为监听套接字
    serverSocket_.setNonblocking();// 设置非阻塞模式 
}

TcpServer::~TcpServer(){}

void TcpServer::start()
{
    // 设置监听事件注册器上的回调函数以及监听套接字的描述符
    spServerChannel_->setFd(serverSocket_.fd());
    spServerChannel_->setReadHandle(std::bind(&TcpServer::onNewConnection, this));
    spServerChannel_->setErrorHandle(std::bind(&TcpServer::onConnectionError, this)); 

    eventLoopThreadPool_->start();
    spServerChannel_->setEvents(EPOLLIN | EPOLLET);
    loop_->addToPoller(spServerChannel_);

    if (removeIdleConnection_)
        loop_->setOnTimeCallback(1, std::bind(&TcpServer::onTime, this));
}

void TcpServer::onNewConnection()
{
    struct sockaddr_in clientaddr;
    int clientfd;
    while( (clientfd = serverSocket_.accept(clientaddr)) > 0) 
    {

        if(connCount_+1 > MAXCONNECTION)
        {
            close(clientfd);
            continue;
        }
        setNonblocking(clientfd);
        setSocketNodelay(clientfd);

        EventLoop *loop = eventLoopThreadPool_->getNextLoop();
        
        SP_TcpConnection spTcpConn = std::make_shared<TcpConnection>(loop, clientfd, clientaddr);
        if (removeIdleConnection_)
        {
            timeWheel_.addConnection(spTcpConn);
            spTcpConn->setIsActiveCallback(std::bind(&TcpServer::isActive, this, _1));
        }
    
        tcpList_[clientfd] = spTcpConn;
        newConnectionCallback_(spTcpConn, loop);// HTTP层分配HttpData块，并且将其与TcpConnection绑定
        
        
        spTcpConn->setConnectionCleanUp(std::bind(&TcpServer::connectionCleanUp, this, clientfd));
    
        spTcpConn->addChannelToLoop();
        ++ connCount_;
        //std::cout<<tcpList_.size()<<std::endl;
        //std::cout << "Connections' number = " << connCount_ << std::endl;
    }
}

void TcpServer::connectionCleanUp(int fd) 
{
    if (loop_->isInLoopThread()) 
    {
        --connCount_;
        tcpList_.erase(fd);
    }
    else
    {  
        loop_->queueInLoop(std::bind(&TcpServer::connectionCleanUp, this, fd));
    }
}

// 每秒计时会更新时间轮
void TcpServer::onTime()
{
    loop_->assertInLoopThread();
    timeWheel_.rotateTimeWheel();
}
// 服务器端的等待连接套接字发生错误，会关闭套接字
void TcpServer::onConnectionError()
{    
    LOG<< "UNKNOWN EVENT" ;
    serverSocket_.close();
}
// 若Tcp连接处于活跃状态，在计时到达的时候调用重新更新TCP连接信息到时间轮
void TcpServer::isActive(SP_TcpConnection spTcpConn) 
{
    if (loop_->isInLoopThread()) 
    {
        if (spTcpConn->isConnect()) 
        {
            timeWheel_.addConnection(spTcpConn);
        }
    } 
    else 
    {
        loop_->queueInLoop(std::bind(&TcpServer::isActive, this, spTcpConn));
    }
}
/* 设置套接字为非阻塞 */
void setNonblocking(int fd)
{
    int opts = fcntl(fd, F_GETFL);// 获取当前文件描述符的信息
    if (opts < 0)
    {
        LOG<<"fcntl(fd,GETFL)";
        exit(1);
    }
    if (fcntl(fd, F_SETFL, opts | O_NONBLOCK) < 0)// 设置当前文件描述符为非阻塞 
    {
        LOG<<"fcntl(fd,SETFL,opts)";
        exit(1);
    }
}

void setSocketNodelay(int fd)
{
	int enable = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable));
}
