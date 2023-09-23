#ifndef __NETKIT_EPOLL_CONNECTION_H__
#define __NETKIT_EPOLL_CONNECTION_H__

#include "read_handler.h"
#include "eventfd_handler.h"
#include "netkit/connection_info.h"
#include "netkit/retcode.h"
#include "logger/logger.h"

namespace netkit { namespace epoll {

class Connection final {
public:
    Connection()
        : m_fd(-1)
        , m_written_fd(-1)
        , m_shutdown_fd(-1)
        , m_epfd(-1)
        , m_fd_added(false)
        , m_written_fd_added(false)
        , m_shutdown_fd_added(false)
        , m_logger(nullptr) {}
    ~Connection();

    const ConnectionInfo& GetInfo() const {
        return m_info;
    }

    /**
       @brief only one reading operation can be outstanding at any given time. That is,
       one MUST wait to receive a `tag` before calling `ReadAsync` again.
    */
    RetCode ReadAsync(void* buf, uint64_t sz, void* tag);

    /**
       @brief only one writing operation can be outstanding at any given time. That is,
       one MUST wait to receive a `tag` before calling `WriteAsync` again.
    */
    RetCode WriteAsync(const void* buf, uint64_t sz, void* tag);

    RetCode ShutDownAsync(void* tag);

private:
    friend class ConnectionManager;
    RetCode Init(int fd, int epfd, Logger* logger);

private:
    int m_fd;
    int m_written_fd, m_shutdown_fd;
    int m_epfd;
    ReadHandler m_read_handler;
    EventfdHandler m_written_handler;
    EventfdHandler m_shutdown_handler;
    bool m_fd_added, m_written_fd_added, m_shutdown_fd_added;
    Logger* m_logger;
    ConnectionInfo m_info;

private:
    Connection(const Connection&) = delete;
    Connection(Connection&&) = delete;
    void operator=(const Connection&) = delete;
    void operator=(Connection&&) = delete;
};

}}

#endif
