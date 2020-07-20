#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H
#include <functional>
#include <string>
#include <memory>
#include <netinet/in.h>
#include "Socket.h"

class HttpData;
class EventLoop;
class Channel;
class Entry;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    friend HttpData;
    typedef uint32_t event_t;
    typedef std::shared_ptr<Channel> SP_Channel;
    typedef std::weak_ptr<Entry> WP_Entry;
    typedef std::function<void()> Callback;
    typedef std::function<void(std::string&)> HandleMessageCallback;// 上层Http业务处理函数
    typedef std::function<void()> TaskCallback;
    typedef std::function<void(std::shared_ptr<TcpConnection>)> IsActiveCallback;
    
    TcpConnection(EventLoop *loop, int fd, struct sockaddr_in clientaddr);
    ~TcpConnection();

    int fd() const
    { 
        return socket_.fd(); 
    }
    bool isConnect() const
    {
        return connected_;
    }
    void addChannelToLoop();
    
    void notifyHttpClosed()
    {
        httpClosed_ = true;
    }

    // 设置HTTP层回调函数
    void setHandleMessageCallback(const HandleMessageCallback &&cb)
    {
        handleMessageCallback_ = cb;
    }
    void setSendCompleteCallback(const Callback &&cb)
    {
        sendCompleteCallback_ = cb;
    }
    void setCloseCallback(const Callback &&cb)
    {
        closeCallback_ = cb;
    }
    void setHalfCloseCallback(const Callback &&cb)
    {
        halfCloseCallback_ = cb;
    }
    void setErrorCallback(const Callback &&cb)
    {
        errorCallback_ = cb;
    }
    // 连接清理函数(由IO线程推送到主线程所持有的事件分发器任务队列)
    void setConnectionCleanUp(const TaskCallback &&cb)
    {
        connectionCleanUp_ = cb;
    }
    // 更新TCP服务器上的时间轮回调函数
    void setIsActiveCallback(const IsActiveCallback && cb)
    {
        isActiveCallback_ = cb;
    }
    void checkWhetherActive();

    //void setChannelOut();
    
    void forceClose();
private:
    // TCP层的处理函数
    void handleRead();
    void handleWrite();
    void handleError();
    void handleClose();
    int recvn(int fd, std::string &bufferIn);
    int sendn(int fd, std::string &bufferOut);

    EventLoop *loop_;//当前的事件循环
    SP_Channel spChannel_;//当前channel，对于confd
    Socket socket_;//连接套接字
    struct sockaddr_in clientaddr_;//连接用户地址

    bool halfClose_; // 半关闭标志位
    bool connected_; // 连接标志位
    bool active_; // 当前连接是否活跃
    bool httpClosed_;//关闭标志位

    // 读写缓冲
    std::string bufferIn_;
    std::string bufferOut_;
    
    // 上层HTTP的回调函数
    HandleMessageCallback handleMessageCallback_;
    Callback sendCompleteCallback_;
    Callback closeCallback_;
    Callback halfCloseCallback_;
    Callback errorCallback_;

    // TCP服务器处理函数
    TaskCallback connectionCleanUp_;
    IsActiveCallback isActiveCallback_;

    static const int BUFSIZE =4096;
};

#endif 