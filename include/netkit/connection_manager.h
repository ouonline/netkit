#ifndef __NETKIT_CONNECTION_MANAGER_H__
#define __NETKIT_CONNECTION_MANAGER_H__

#include "retcode.h"
#include "connection.h"
#include "liburing.h"
#include "logger/logger.h"
#include <stdint.h>
#include <functional>

namespace netkit {

class ConnectionManager final {
public:
    ConnectionManager(Logger* logger) : m_logger(logger) {}
    ~ConnectionManager();

    RetCode Init();

    RetCode CreateTcpServer(const char* addr, uint16_t port, void* tag);
    RetCode CreateTcpClient(const char* addr, uint16_t port, Connection*);

    /** @brief usually called in the server side after new clients are accepted. */
    void InitializeConnection(int client_fd, Connection*);

    /**
       @param func used to handle events:
         - CONNECTED: `res` is the client fd, `tag` is the value passed to `CreateTcpServer()`
         - SEND: `res` is the number of bytes sent, `tag` is the value passed to `Connection::SendAsync()`.
         - RECV: `res` is the number of bytes received, `tag` is the value passed to `Connection::RecvAsync()`.
         - SHUTDOWN: `res` is unused, `tag` is the value passed to `Connection::ShutDownAsync()`.

       @note also note that callers should handle client disconnection when `res` is 0 in SEND or RECV.
    */
    RetCode Loop(const std::function<void(uint64_t res, void* tag)>& func);

private:
    Logger* m_logger;
    struct io_uring m_ring;

private:
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager(ConnectionManager&&) = delete;
    void operator=(const ConnectionManager&) = delete;
    void operator=(ConnectionManager&&) = delete;
};

}

#endif
