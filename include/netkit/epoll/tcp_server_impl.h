#ifndef __NETKIT_EPOLL_TCP_SERVER_IMPL_H__
#define __NETKIT_EPOLL_TCP_SERVER_IMPL_H__

#include "netkit/tcp_server.h"
#include "event_handler.h"
#include "logger/logger.h"

namespace netkit { namespace epoll {

class TcpServerImpl final : public EventHandler, public TcpServer {
public:
    TcpServerImpl() : m_fd(-1) {}

    TcpServerImpl(TcpServerImpl&& svr) {
        m_fd = svr.m_fd;
        tag = svr.tag;
        svr.m_fd = -1;
        svr.tag = nullptr;
    }

    ~TcpServerImpl() {
        Destroy();
    }

    RetCode Init(const char* addr, uint16_t port, Logger*);
    void Destroy();

    RetCode MultiAcceptAsync(void* tag, NotificationQueue*) override;

private:
    int64_t In() override;

private:
    int m_fd;
    Logger* m_logger;

private:
    TcpServerImpl(const TcpServerImpl&) = delete;
    void operator=(const TcpServerImpl&) = delete;
    void operator=(TcpServerImpl&&) = delete;
};

}}

#endif
