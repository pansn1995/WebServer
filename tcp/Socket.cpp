#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <cstdlib>
#include <cstring>
#include "Socket.h"

// Socket()构造函数创建一个套接字
// Socket(int fd)构造函数封装一个描述符为fd的套接字
Socket::Socket()
{
    closed_ = true;
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == fd_)
    {
        perror("socket create fail!");
        exit(-1);
    }
    closed_ = false;
}

Socket::~Socket()
{
    if (!closed_) {
        ::close(fd_);
    }
}
// 将socket设置为地址可复用
void Socket::setReuseAddr()
{
    int on = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
}

void Socket::setNonblocking()
{
    int opts = fcntl(fd_, F_GETFL,0);
    if (opts<0)
    {
        perror("fcntl(serverfd_,GETFL)");
        exit(1);
    }
    if (fcntl(fd_, F_SETFL, opts | O_NONBLOCK) < 0)
    {
        perror("fcntl(serverfd_,SETFL,opts)");
        exit(1);
    }
}

void Socket::setSocketNolinger()
{
    struct linger linger_;
	linger_.l_onoff = 1;
	linger_.l_linger = 30;
	setsockopt(fd_, SOL_SOCKET, SO_LINGER, (const char*) &linger_, sizeof(linger_));
}

bool Socket::bindAddress(int port)
{
    struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(port);
	int resval = bind(fd_, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (resval == -1)
	{
		::close(fd_);
        closed_ = true;
		perror("error bind");
		exit(1);
	}
    return true;
}

bool Socket::listen()
{    
	if (::listen(fd_, 2048) < 0)
	{
		perror("error listen");
        closed_ = true;
		::close(fd_);
		exit(1);
	}
    return true;
}

int Socket::accept(struct sockaddr_in &clientaddr)
{
    socklen_t lengthofsockaddr = sizeof(clientaddr);
    int clientfd = ::accept(fd_, (struct sockaddr*)&clientaddr, &lengthofsockaddr);
    if (clientfd < 0) 
    {
        return clientfd;
	}
    return clientfd;
}
//为避免重复关闭, 第一次关闭需设置closed_
bool Socket::close()
{
    if (!closed_) 
    {
        ::close(fd_);
        closed_ = true;
    }
    return true;
}