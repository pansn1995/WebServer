#include "HttpData.h"
#include <fcntl.h>
#include <sstream>
#include <sys/mman.h>
#include <exception>
#include <sys/stat.h>
#include "../tcp/TcpConnection.h"
#include "../eventloop/EventLoop.h"
#include <time.h>

pthread_once_t MimeType::once_control = PTHREAD_ONCE_INIT;
std::unordered_map<std::string, std::string> MimeType::mime;

void MimeType::init() {
    mime[".html"] = "text/html";
    mime[".avi"] = "video/x-msvideo";
    mime[".bmp"] = "image/bmp";
    mime[".c"] = "text/plain";
    mime[".doc"] = "application/msword";
    mime[".gif"] = "image/gif";
    mime[".gz"] = "application/x-gzip";
    mime[".htm"] = "text/html";
    mime[".ico"] = "image/x-icon";
    mime[".jpg"] = "image/jpeg";
    mime[".png"] = "image/png";
    mime[".txt"] = "text/plain";
    mime[".mp3"] = "audio/mp3";
    mime[".js"]="application/javascript";
    mime[".css"]="text/css";
    mime[".json"]="application/json";
    mime["default"] = "text/html";
}

std::string MimeType::getMime(const std::string &suffix) {
    pthread_once(&once_control, MimeType::init);
    if (mime.find(suffix) == mime.end())
        return mime["default"];
    else
        return mime[suffix];
}

HttpData::HttpData(SP_TcpConnection spTcpConn, EventLoop *loop)
    : loop_(loop),
      spCurrPro_(),
      currMethod_(),
      completed_(false),
      remain_(0),
      crlfcrlfPos_(std::string::npos),
      keepAlive_(false),
      connected_(true),
      wpTcpConn_(spTcpConn)
{
}

/* 处理消息的方式
 * 接收到消息时，HTTP层的处理工作: 主要负责解析HTTP请求报文并做出响应 
 * 注意: 此函数在IO线程中运行 */
void HttpData::handleMessage(std::string &s)
{
    std::string str;
    str.swap(s);
    // 用于防止在执行该函数期间HttpData资源块被释放。
    std::shared_ptr<HttpData> guard = shared_from_this();

    // 若当前Http会话关闭，则将接收到的信息丢弃并返回
    if (!connected_) 
    {
        LOG << "Http session closed!" ;
        return;
    }

    if (str.empty()) 
    {   // 下层TCP连接接收到FIN(只从TCP缓冲区中提取单独的文件结束符)
        connected_ = false;// 关闭HTTP层
        SP_TcpConnection spTcpConn = wpTcpConn_.lock();
        if (spTcpConn) 
        {
            // HTTP层工作已完成，TCP层可在发送完数据后关闭。
            spTcpConn->notifyHttpClosed();
            spTcpConn->handleClose();
        }
        else
        {
            LOG<< "handleMessage: TcpConnection has closed!" ;
        }
        return;
    }
    
    // 把数据加入到HTTP层的缓冲区
    if (recvMsg_.empty())
        recvMsg_.swap(str);
    else
        recvMsg_.append(str);
        
    if (!spCurrPro_) 
        crlfcrlfPos_ = recvMsg_.find("\r\n\r\n");

    // 注意：若当前spCurrPro_非空并且crlfcrlfPos_==std::string::npos
    //       则将recvMsg_中至多remain_个字节的数据附加到spCurrPro指
    //       向的HttpProcessContext
    while (connected_ && !recvMsg_.empty()&& (crlfcrlfPos_ != std::string::npos || spCurrPro_))
    {
        if (parseRcvMsg() != 0)// 分离报文出错
        {
            LOG << "pareRcvMsg error!" ; 
            if(spCurrPro_) 
            {
                // 将差错响应报文压入准备响应队列
                addToBuf();// 向客户端发送报文 
                crlfcrlfPos_ = std::string::npos;
                spCurrPro_.reset();// 释放资源
            }
            break;
        }

        if (spCurrPro_ && completed_) // 接收到完整报文
        {
            // 推进线程或者在当前线程工作
            //LOG<<spCurrPro_->requestContext;
            handleMessageTask(spCurrPro_);
            crlfcrlfPos_ = recvMsg_.find("\r\n\r\n");// 重置
            spCurrPro_.reset();
            completed_ = false;
        }else if (spCurrPro_ && !completed_)// 接收到POST报文，且不完整
        {
            crlfcrlfPos_ = std::string::npos;
        }else{
            // 出现错误(不可能执行到此处)
            LOG << "handle message error-- parseRcvMsg_() error" ;
            recvMsg_.clear();
            return;
        }
    }
}

int HttpData::parseRcvMsg()
{
    if (!spCurrPro_)// 新报文
    {
        SP_HttpProcessContext spProcessContext =std::make_shared<HttpProcessContext>();
        spCurrPro_ = spProcessContext;;

        // 分离命令
        size_type methodPos = recvMsg_.find(std::string(" "));
        std::string method = recvMsg_.substr(0, methodPos);

        if (method == "GET")
            currMethod_ = METHOD_GET;
        else if (method == "POST")
            currMethod_ = METHOD_POST;
        else if(method == "HEAD")
            currMethod_ = METHOD_HEAD;
        else{
            LOG<< "HttpData::parseRcvMsg--don't support method " << method ;
            httpError(501, "Method Not Implemented", spCurrPro_);
            return -1;
        }
    
        if (currMethod_ == METHOD_GET||currMethod_==METHOD_HEAD)
        {
            if (crlfcrlfPos_+4 == recvMsg_.size()) 
            {
                spCurrPro_->requestContext.swap(recvMsg_);
                completed_ = true;
            } else{ // 当前缓冲区有多于一个报文
                std::string str = recvMsg_.substr(0, crlfcrlfPos_ + 4);
                recvMsg_ = recvMsg_.substr(crlfcrlfPos_ + 4);
                spCurrPro_->requestContext.swap(str);
                completed_ = true;
            }
        }else if (currMethod_ == METHOD_POST)// 当前请求报文为POST类型
        {
            size_type conLen_pos= recvMsg_.find(std::string("Content-Length"));
            if (conLen_pos == std::string::npos)
            {
                LOG<< "HttpData::parseRecvMsg--POST request context without Content-Length" ;
                httpError(400, "Bad request!", spCurrPro_);
                return -1;
            }

            // 查找Content-Length的结束尾部
            size_type crlf_pos = recvMsg_.find(std::string("\r\n"), conLen_pos);// 从conLen_pos位置起寻找第一个“\r\n”
            std::string bodyLengthStr = recvMsg_.substr(conLen_pos+16, crlf_pos-conLen_pos-16);
            // 将bodyLength转换为数值
            int bodyLength = 0;
            try 
            {
                bodyLength = stoi(bodyLengthStr);
            }catch (std::logic_error &e) 
            {
                // 数值解析错误
                LOG << "The value of Content-Length is wrong!" ;
                httpError(400, "Bad request: The format of content length is wrong!", spCurrPro_);
                return -1;
            }

            auto resDataNum = recvMsg_.size() - (crlfcrlfPos_+4);

            if (bodyLength <= static_cast<int>(resDataNum))// 若缓冲区中携带完整POST报文
            {
                std::string str = recvMsg_.substr(0, crlfcrlfPos_ + 4 + bodyLength);
                recvMsg_ = recvMsg_.substr(crlfcrlfPos_ + 4 + bodyLength);
                spCurrPro_->requestContext.swap(str);
                remain_ = 0;
                completed_ = true;
            } else// 请求报文携带数据不完整
            {
                (spCurrPro_->requestContext).swap(recvMsg_);
                remain_ = bodyLength - resDataNum;
                completed_ = false;
            }
        }
    }else// 当前有正在处理的报文，即上一次报文收取不完整(POST)
    {
        //std::cout<<1<<std::endl;
        if (remain_ <= recvMsg_.size()) //这次能够得到完整报文
        {
            std::string str = recvMsg_.substr(0, remain_);
            (spCurrPro_->requestContext).append(str);
            recvMsg_ = recvMsg_.substr(remain_);
            completed_ = true;
            remain_ = 0;
        }else{ 
            (spCurrPro_->requestContext).append(recvMsg_);
            remain_-=recvMsg_.size();
            recvMsg_.clear();
        }
    } 
    return 0;
}

// 报文处理任务(可在IO线程或工作线程中运行)
void HttpData::handleMessageTask(SP_HttpProcessContext spProContext)
{
    if (-1 != parseHttpRequest(spProContext))
        httpProcess(spProContext);
    else
        return ;

    keepAlive_ = spProContext->keepAlive;
    addToBuf();
}

int HttpData::parseRequestLine(SP_HttpProcessContext spProContext)
{
    std::string &request = spProContext->requestContext;
    size_type crlf_pos = request.find("\r\n");
    if (crlf_pos == std::string::npos)
    {
        httpError(400, "Wrong request!", spProContext);
        return -1;
    }
    std::string requestLine = request.substr(0, crlf_pos);
    request = request.substr(crlf_pos+2);
    std::stringstream sstream(requestLine);
    ParseRequestLineState state = PARSE_METHOD;
    std::string tmp;
    while (sstream >> tmp) 
    {
	    switch (state) 
        {
            case PARSE_METHOD:
            {
                spProContext->method = tmp;
                state = PARSE_URI;
                break;
            }
            case PARSE_URI:
            {
	            spProContext->url = tmp;
                state = PARSE_VERSION;
                break;
            }
            case PARSE_VERSION:
            {
                spProContext->version = tmp;
                state = PARSE_SUCCESS;
                break;
            }
            case PARSE_SUCCESS:
            {
                state = PARSE_ERROR;
                httpError(400, "Wrong request!", spProContext);
                return -1;
            }
            default:
            {
                break;
            }
        }
    }
    return 0;
}

int HttpData::parseRequestHeader(SP_HttpProcessContext spProContext)
{
    std::string &request = spProContext->requestContext;
    std::unordered_map<std::string, std::string> &header = spProContext->header;
    size_type start = 0;
    size_type pos;
    std::string key;
    std::string value;
    ParseRequestHeaderState state = H_KEY;
    while (state != H_END) 
    {
        switch(state) 
        {
            case H_KEY:
            {
                pos = request.find(":", start);
                if (pos != std::string::npos) 
                {
                    key = request.substr(start, pos-start);
                    ++pos;
                    while (pos < request.size() && request[pos] == ' ') ++pos;
                    start = pos;
                    state = H_VALUE;
                }
                else
                {
                    state = H_ERROR;
                }
                break;
            }
            case H_VALUE:
            {
                pos = request.find("\r\n", start);
                if (pos != std::string::npos) 
                {
                    value = request.substr(start, pos-start);
                    header[key] = value;
                    start = pos+2;
                    state = H_SUCCESS;
                }
                else
                {
                    state = H_ERROR;
                }
                break;
            }
            case H_SUCCESS:
            {
                if (start > request.size()-2) 
                {
                    state = H_ERROR;
                }
                else
                {
                    request[start] == '\r' && request[start+1] == '\n' ? state = H_END : state = H_KEY;
                    if (state == H_END)
                        start = start+2;
                }
                break;
            }
            case H_ERROR:
            {
                httpError(400, "Wrong request!", spProContext);
                return -1;
            }
            default:
            {
                break;
            }
        }
    }
    request = request.substr(start);
    return 0;
}

int HttpData::parseRequestBody(SP_HttpProcessContext spProContext)
{
    //剩余部分都是实体主体
   (spProContext->body).swap(spProContext->requestContext);
   return 0;
}

int HttpData::parseHttpRequest(SP_HttpProcessContext spProContext)
{
    if (-1 == parseRequestLine(spProContext))
    {
        LOG<<"parseRequestLine fail";
        return -1;
    }
    if (-1 == parseRequestHeader(spProContext)) 
    {
        LOG<<"parseRequestHeader fail";
        return -1;
    }
    parseRequestBody(spProContext);
    // 解析成功
    return 0;
}

/* 根据分解后的HTTP请求报文，进行处理(构建HTTP响应报文) */
void HttpData::httpProcess(SP_HttpProcessContext spProContext)
{
    if(spProContext->method=="GET"||spProContext->method=="HEAD")
    {
        std::string path;
        //std::string queryString;
        std::string &responseContext = spProContext->responseContext;

        size_t pos = spProContext->url.find("?");
        /* 将url中的路径与参数分离 */
        if(pos != std::string::npos)
        {
            path = spProContext->url.substr(0, pos);
            //queryString = spProContext->url.substr(pos+1);
        }else
        {
            path = spProContext->url;
        }
        
        //keepalive判断处理
        auto iter = spProContext->header.find("Connection");
        if(iter != spProContext->header.end())
        {
            spProContext->keepAlive = (iter->second == "Keep-Alive");
        }else
        {
            if(spProContext->version == "HTTP/1.1")
            {
                spProContext->keepAlive = true;//HTTP/1.1默认长连接
            } else
            {
                spProContext->keepAlive = false;//HTTP/1.0默认短连接
            }            
        }

        if("/" == path)// 路径为根目录，返回主页
        {        
            path = "/index.html";
        } else if("/hello" == path)
        {
            //Wenbbench 测试用
            std::string responseBody;
            std::string fileType("text/html");
            responseBody = ("hello world");
            responseContext += spProContext->version + " 200 OK\r\n";// 响应报文的响应行
            responseContext += "Server: Pan's WebServer\r\n";// 首部
            responseContext += "Content-Type: " + fileType + "; charset=utf-8\r\n";
            if(iter != spProContext->header.end())// 若请求报文中存在Connection选项
            {
                responseContext += "Connection: " + iter->second + "\r\n";
            }
            responseContext += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";// 存放响应报文的大小
            responseContext += "\r\n";// 首部结束，下面是响应报文的主体
            responseContext.append(responseBody);
            spProContext -> success = true;
            return;
        } 

        int dot_pos = path.find('.');
        std::string filetype;
        if (dot_pos < 0)
            filetype = MimeType::getMime("default");
        else
            filetype = MimeType::getMime(path.substr(dot_pos));
        path=rootFileName+path;
        //std::cout<<path<<std::endl;
        //path.insert(0,".");
        struct stat sbuf;
        if (stat(path.c_str(), &sbuf) < 0) {
            spProContext->success = false;
            httpError(404, "Not Found", spProContext);
            return ;
        }
        responseContext += spProContext->version + " 200 OK\r\n";
        responseContext += "Server: Pansn's WebServer\r\n";
        responseContext += "Content-Type: " + filetype + "\r\n";
        responseContext += "Content-Length: " + std::to_string(sbuf.st_size) + "\r\n";
        //if(keepAlive())
        //  responseContext += "Connection: Keep-Alive\r\n" ;
        responseContext += "\r\n";

        if(spProContext->method=="HEAD")
        {
            spProContext->success = true;
            return ;
        }
        
        int src_fd = open(path.c_str(), O_RDONLY, 0);
        if (src_fd < 0) {
            spProContext->success = false;
            httpError(404, "Not Found", spProContext);
            return ;
        }
        void *mmapRet = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
        close(src_fd);
        if (mmapRet == (void *)-1) {
            munmap(mmapRet, sbuf.st_size);
            spProContext->success = false;
            httpError(404, "Not Found", spProContext);
            return ;
        }
        
        char *src_addr = static_cast<char *>(mmapRet);
        responseContext += std::string(src_addr, src_addr + sbuf.st_size);
        ;
        munmap(mmapRet, sbuf.st_size);
        // 处理报文成功
        spProContext->success = true;
    }else if(spProContext->method=="POST")
    {
        //std::cout<<2<<std::endl;
    }
}

// 由于只会在IO线程中调用，因此可直接调用TcpConnectino的send操作
void HttpData::addToBuf()
{
    if (connected_) 
    {
        if (!keepAlive() || !spCurrPro_->success) 
        {
            halfClose_ = true;
            connected_ = false;
        }
    }
    // 若当前连接为半关闭，并且处理待处理报文为空，则可关闭连接
    if (halfClose_ )
        connected_ = false;

    SP_TcpConnection spTcpConn = wpTcpConn_.lock();
    if (spTcpConn) 
    {
        if (!connected_) 
            spTcpConn->notifyHttpClosed();
        spTcpConn->bufferOut_.append(spCurrPro_->responseContext);
        //spTcpConn->setChannelOut();
        spTcpConn->handleWrite();
    }
}


void HttpData::handleSendComplete()
{
    // LOG<<"send success";
}

/* 发生在TCP服务器强制关闭连接
 * HTTP层连接关闭时主要进行资源释放操作
 * */
void HttpData::handleClose()
{
    connected_ = false;
}

// 发生在TCP连接接收到FIN分节
void HttpData::handleHalfClose()
{
    halfClose_ = true;
}

/* 底层TCP连接发生错误时，HTTP层处理工作 
 * 关闭当前HTTP层会话 */
void HttpData::handleError()
{
    connected_ = false;
}

// 报文处理出错所调用的函数
void HttpData::httpError(int err_num, std::string short_msg, SP_HttpProcessContext spProContext)
{
    spProContext->success = false;
    std::string responseBody;
    std::string &responseContext = spProContext->responseContext;

    responseBody += "<html><title>出错了</title>";
    responseBody += "<body bgcolor=\"ffffff\"><h1>";
    responseBody += std::to_string(err_num) + " " + short_msg;
    responseBody += "</h1><hr><em> Pansn's WebServer</em>\n</body></html>";

    responseContext += spProContext->version + " " + std::to_string(err_num) + " " + short_msg + "\r\n";
    responseContext += "Content-Type: text/html\r\n";
    responseContext += "Connection: Keep-Alive\r\n";
    responseContext += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";
    responseContext += "\r\n";
    responseContext += responseBody;
}