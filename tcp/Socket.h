#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include "../log/Logging.h"

class Socket
{
public:
    Socket();
    Socket(int fd): fd_(fd), closed_(false) { }
    ~Socket();

    int fd() const { return fd_; }    
    void setReuseAddr();
    void setNonblocking();
    void setSocketNolinger();

    bool bindAddress(int serverport);
    bool listen();
    int accept(struct sockaddr_in &clientaddr);
    bool close();
private:
    int fd_;
    bool closed_;
};

#endif