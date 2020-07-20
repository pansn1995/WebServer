#ifndef HTTPDATA_H
#define HTTPDATA_H
#include <string>
#include <unordered_map>
#include <map>
#include <memory>
#include <sys/epoll.h>
#include <functional>
#include <unistd.h>
#include <queue>

class TcpConnection;
class EventLoop;
class Channel;

const std::string rootFileName="./root/";

enum ParseRequestLineState {
    PARSE_METHOD = 0,
    PARSE_URI,
    PARSE_VERSION,
    PARSE_SUCCESS,
    PARSE_ERROR
};

enum ParseRequestHeaderState {
    H_KEY = 0,
    H_VALUE,
    H_SUCCESS,
    H_END,
    H_ERROR
};

//方法
enum HttpMethod {
    METHOD_POST = 0,
    METHOD_GET,
    METHOD_HEAD ,
    METHOD_OTHER
};

//单例模式
class MimeType {
private:
    static void init();
    static std::unordered_map<std::string, std::string> mime;
    MimeType();
    MimeType(const MimeType &m);

public:
   static std::string getMime(const std::string &suffix);

private:
   static pthread_once_t once_control;
};

struct HttpProcessContext {
    std::string requestContext;// 待解析的请求报文
    std::string responseContext;// 待发送响应报文
	std::string method;
	std::string url;
	std::string version;
	std::unordered_map<std::string, std::string> header;
	std::string body;
    bool keepAlive = false;
    bool success = false;// 标记报文是否成功处理
};// 处理报文

class HttpData : public std::enable_shared_from_this<HttpData> {
public:
    typedef std::shared_ptr<TcpConnection> SP_TcpConnection;
    typedef std::weak_ptr<TcpConnection> WP_TcpConnection;
    typedef std::shared_ptr<HttpProcessContext> SP_HttpProcessContext;
    typedef std::string::size_type size_type;

    HttpData(SP_TcpConnection spTcpConn,EventLoop *loop);
    ~HttpData() {}

    void handleMessage(std::string &s);
    void handleSendComplete();
    void handleClose();
    void handleHalfClose();
    void handleError();

    void handleMessageTask( SP_HttpProcessContext spProContext);
    void notifyIoThreadDataPrepare( SP_HttpProcessContext spProContext);

private:
    /* 分离接收缓冲区的报文 */
    int parseRcvMsg();

    /* 应用状态解析请求报文 */
    int parseRequestLine(SP_HttpProcessContext    spProContext);
    int parseRequestHeader(SP_HttpProcessContext    spProContext);
    int parseRequestBody(SP_HttpProcessContext    spProContext);

    /* 解析HTTP请求报文 */
    int parseHttpRequest(SP_HttpProcessContext    spProContext);

    /* 根据解析后的报文构建HTTP响应报文 */
    void httpProcess(SP_HttpProcessContext    spProContext);

    /* 将响应报文的内容转移到发送缓冲区 */
    void addToBuf();

    /* 报文解析出错 */
    void httpError(int err_num, std::string short_msg,SP_HttpProcessContext    spProContext);
    
    bool keepAlive() const{return keepAlive_; }
private:
    EventLoop *loop_;
    std::string recvMsg_;// 从缓冲区接收的报文
    SP_HttpProcessContext spCurrPro_; // 指向正在处理的报文
    HttpMethod currMethod_; // 当前正在分离的请求报文类型
    bool completed_;// 用于标记当前报文是否完整
    size_type remain_;// 用于记录POST请求还剩余多少数据未读
    size_type crlfcrlfPos_;// 记录“\r\n\r\n”的接收位置
    bool keepAlive_;
    bool connected_ = true;// 标记当前会话是否关闭
    bool halfClose_ = false;
    WP_TcpConnection wpTcpConn_;// 此处持有WP_TcpConnection指针
};
#endif