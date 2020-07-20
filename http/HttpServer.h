#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <memory>
#include "../tcp/TcpServer.h"

class EventLoop;
class TcpConnection;
class HttpData;

class HttpServer { 
public:
    typedef std::shared_ptr<HttpData> SP_HttpData;
    typedef std::shared_ptr<TcpConnection> SP_TcpConnection;

    HttpServer(EventLoop *loop, int port, int ioThreadNum, int idleSeconds);
    ~HttpServer();
    void start();
private:
    /* 新建连接时所进行的处理(注册至tcpserver_, 并在tcpserver_中的回调函数被调用) */
    void handleNewConnection(SP_TcpConnection spTcpConn, EventLoop *loop); 
    TcpServer tcpServer_;
};

#endif