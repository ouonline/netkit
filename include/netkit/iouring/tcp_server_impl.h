#ifndef __NETKIT_IOURING_TCP_SERVER_IMPL_H__
#define __NETKIT_IOURING_TCP_SERVER_IMPL_H__

#include "netkit/tcp_server.h"
#include "logger/logger.h"
#include <stdint.h>

namespace netkit { namespace iouring {

class TcpServerImpl final : public TcpServer {
public:
    TcpServerImpl() : m_fd(-1) {}

    TcpServerImpl(TcpServerImpl&& svr) {
        m_fd = svr.m_fd;
        svr.m_fd = -1;
    }

    ~TcpServerImpl() {
        Destroy();
    }

    RetCode Init(const char* addr, uint16_t port, Logger* l);
    void Destroy();

    RetCode MultiAcceptAsync(void* tag, NotificationQueue*) override;

private:
    int m_fd;

private:
    TcpServerImpl(const TcpServerImpl&) = delete;
    void operator=(const TcpServerImpl&) = delete;
    void operator=(TcpServerImpl&&) = delete;
};

}}

#endif
