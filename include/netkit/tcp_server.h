#ifndef __NETKIT_TCP_SERVER_H__
#define __NETKIT_TCP_SERVER_H__

#include <sys/socket.h> // shutdown()
#include <unistd.h> // close()

namespace netkit {

class TcpServer final {
public:
    TcpServer(int fd) : m_fd(fd) {}
    TcpServer(TcpServer&& svr) {
        m_fd = svr.m_fd;
        svr.m_fd = -1;
    }

    bool IsValid() const {
        return (m_fd > 0);
    }

    void ShutDown() {
        shutdown(m_fd, SHUT_RDWR);
        close(m_fd);
    }

private:
    int m_fd;

private:
    TcpServer(const TcpServer&) = delete;
    void operator=(const TcpServer&) = delete;
    void operator=(TcpServer&&) = delete;
};

}

#endif
