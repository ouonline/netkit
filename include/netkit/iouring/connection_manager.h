#ifndef __NETKIT_IOURING_CONNECTION_MANAGER_H__
#define __NETKIT_IOURING_CONNECTION_MANAGER_H__

#include "connection.h"
#include "netkit/retcode.h"
#include "netkit/tcp_server.h"
#include "liburing.h"
#include "logger/logger.h"

namespace netkit { namespace iouring {

class ConnectionManager final {
public:
    ConnectionManager(Logger* logger) : m_logger(logger) {}
    ~ConnectionManager();

    RetCode Init();

    TcpServer CreateTcpServer(const char* addr, uint16_t port, void* tag);
    RetCode CreateTcpClient(const char* addr, uint16_t port, Connection*);

    /** @brief usually called in the server side after new clients are accepted. */
    RetCode InitializeConnection(int client_fd, Connection*);

    /**
       @param `res` has different meanings according to events:
         - CONNECTED: `res` is the client fd or -errno, `tag` is the value passed to `CreateTcpServer()`
         - SEND: `res` is the number of bytes sent or -errno, `tag` is the value passed to `Connection::WriteAsync()`.
         - RECV: `res` is the number of bytes received or -errno, `tag` is the value passed to `Connection::ReadAsync()`.
         - SHUTDOWN: `res` is the return value of `shutdown()`, `tag` is the value passed to `Connection::ShutDownAsync()`.

       @note also note that callers should handle client states when `res` is non-positive.
    */
    RetCode Wait(int64_t* res, void** tag);

private:
    Logger* m_logger;
    struct io_uring m_ring;

private:
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager(ConnectionManager&&) = delete;
    void operator=(const ConnectionManager&) = delete;
    void operator=(ConnectionManager&&) = delete;
};

}}

#endif
