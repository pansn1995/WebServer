#ifndef TCP_SERVER_H
#define TCP_SERVER_H
#include <functional>
#include <memory>
#include <map>
#include "Socket.h"
#include "../eventloop/EventLoopThreadPool.h"
#include "TcpConnection.h"
#include "../timer/TimeWheel.h"

class Channel;
class EventLoop;
  
class TcpServer
{
public:
    typedef std::shared_ptr<TcpConnection> SP_TcpConnection;
    typedef std::shared_ptr<Channel> SP_Channel;
    typedef std::function<void(SP_TcpConnection, EventLoop*)> Callback;

    TcpServer(EventLoop* loop, int port, int threadNum = 0, int idleSeconds = 0);
    ~TcpServer();

    void start();

    void setNewConnCallback(Callback &&cb)
    {
        newConnectionCallback_ = cb; 
    }
private:
    // 连接清理工作
    void connectionCleanUp(int fd);
    // 监听套接字发生错误处理工作
    void onConnectionError();
    // 连接到来处理工作
    void onNewConnection();

    Socket serverSocket_;// TCP服务器的监听套接字
    EventLoop *loop_;// 所绑定的EventLoop
    std::unique_ptr<EventLoopThreadPool> eventLoopThreadPool_;//线程池
    SP_Channel spServerChannel_;// 监听连接事件注册器
    std::map<int, SP_TcpConnection> tcpList_;// 记录TCP server上的当前连接，并控制TcpConnection资源块的释放
    int connCount_;//已经连接的用户数
    // 业务接口函数(上层Http的处理业务回调函数)
    Callback newConnectionCallback_;

    TimeWheel timeWheel_;//时间轮
    bool removeIdleConnection_;
     
    void onTime();
    // TCP连接发送信息或者接收数据时，更新时间轮中的数据
    void isActive(SP_TcpConnection spTcpConn);

    static const int MAXCONNECTION =65536 ;
};

#endif