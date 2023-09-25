#ifndef __NETKIT_IOURING_TCP_CLIENT_IMPL_H__
#define __NETKIT_IOURING_TCP_CLIENT_IMPL_H__

#include "netkit/tcp_client.h"
#include "netkit/retcode.h"
#include "logger/logger.h"

namespace netkit { namespace iouring {

class TcpClientImpl final : public TcpClient {
public:
    TcpClientImpl() {}

    TcpClientImpl(TcpClientImpl&& c) {
        m_fd = c.m_fd;
        c.m_fd = -1;
    }

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

private:
    TcpClientImpl(const TcpClientImpl&) = delete;
    void operator=(const TcpClientImpl&) = delete;
    void operator=(TcpClientImpl&&) = delete;
};

}}

#endif
