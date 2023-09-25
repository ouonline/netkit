#ifndef __NETKIT_EPOLL_TCP_CLIENT_IMPL_H__
#define __NETKIT_EPOLL_TCP_CLIENT_IMPL_H__

#include "netkit/tcp_client.h"
#include "read_handler.h"
#include "eventfd_handler.h"
#include "logger/logger.h"
#include <stdint.h>

namespace netkit { namespace epoll {

class TcpClientImpl final : public TcpClient {
public:
    TcpClientImpl()
        : m_fd(-1)
        , m_written_fd(-1)
        , m_shutdown_fd(-1)
        , m_fd_added(false)
        , m_written_fd_added(false)
        , m_shutdown_fd_added(false)
        , m_logger(nullptr) {}

    TcpClientImpl(TcpClientImpl&&);

    ~TcpClientImpl() {
        Destroy();
    }

    /** @brief `fd` will be closed in destructor. */
    RetCode Init(int fd, Logger*);
    RetCode Init(const char* addr, uint16_t port, Logger*);
    void Destroy();

    RetCode ReadAsync(void* buf, uint64_t sz, void* tag, NotificationQueue*) override;
    RetCode WriteAsync(const void* buf, uint64_t sz, void* tag, NotificationQueue*) override;
    RetCode ShutDownAsync(void* tag, NotificationQueue*) override;

private:
    int m_fd;
    int m_written_fd, m_shutdown_fd;
    ReadHandler m_read_handler;
    EventfdHandler m_written_handler;
    EventfdHandler m_shutdown_handler;
    bool m_fd_added, m_written_fd_added, m_shutdown_fd_added;
    Logger* m_logger;

private:
    TcpClientImpl(const TcpClientImpl&) = delete;
    void operator=(const TcpClientImpl&) = delete;
    void operator=(TcpClientImpl&&) = delete;
};

}}

#endif
