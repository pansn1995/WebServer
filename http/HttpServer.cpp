#include "HttpData.h"
#include "HttpServer.h"
#include "../tcp/TcpConnection.h"
using namespace std::placeholders;

HttpServer::HttpServer(EventLoop *loop, int port, int ioThreadNum, int idleSeconds)
    : tcpServer_(loop, port, ioThreadNum, idleSeconds)
{}

HttpServer::~HttpServer(){}


void HttpServer::handleNewConnection(SP_TcpConnection spTcpConn, EventLoop *loop)
{
    SP_HttpData spHttpData=std::make_shared<HttpData>(new HttpData(spTcpConn, loop));

    //将一份智能指针实参绑定到回调函数中，使得由sptcpconn管理httpdata的生命周期
    spTcpConn->setHandleMessageCallback(std::bind(&HttpData::handleMessage, spHttpData, _1));
    spTcpConn->setSendCompleteCallback(std::bind(&HttpData::handleSendComplete, spHttpData));
    spTcpConn->setCloseCallback(std::bind(&HttpData::handleClose, spHttpData));
    spTcpConn->setHalfCloseCallback(std::bind(&HttpData::handleHalfClose, spHttpData));
    spTcpConn->setErrorCallback(std::bind(&HttpData::handleError, spHttpData));
}

void HttpServer::start()
{
    tcpServer_.setNewConnCallback(std::bind(&HttpServer::handleNewConnection, this, _1, _2));
    tcpServer_.start();
}
