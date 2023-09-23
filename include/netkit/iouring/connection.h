#ifndef __NETKIT_IOURING_CONNECTION_H__
#define __NETKIT_IOURING_CONNECTION_H__

#include "netkit/connection_info.h"
#include "netkit/retcode.h"
#include "liburing.h"
#include "logger/logger.h"

namespace netkit { namespace iouring {

class Connection final {
public:
    Connection() : m_fd(-1), m_ring(nullptr), m_logger(nullptr) {}
    ~Connection();

    const ConnectionInfo& GetInfo() const {
        return m_info;
    }

    /**
       @brief only one reading operation can be outstanding at any given time. That is,
       callers MUST wait to receive a `tag` before calling `ReadAsync` again.
    */
    RetCode ReadAsync(void* buf, uint64_t sz, void* tag);

    /**
       @brief only one writing operation can be outstanding at any given time. That is,
       callers MUST wait to receive a `tag` before calling `WriteAsync` again.
    */
    RetCode WriteAsync(const void* buf, uint64_t sz, void* tag);

    RetCode ShutDownAsync(void* tag);

private:
    friend class ConnectionManager;
    RetCode Init(int fd, struct io_uring* ring, Logger* logger);

private:
    int m_fd;
    struct io_uring* m_ring;
    Logger* m_logger;
    ConnectionInfo m_info;

private:
    Connection(const Connection&) = delete;
    void operator=(const Connection&) = delete;
    void operator=(Connection&&) = delete;
};

}}

#endif
