#ifndef __NETKIT_EPOLL_CONNECTION_MANAGER_H__
#define __NETKIT_EPOLL_CONNECTION_MANAGER_H__

#include "connection.h"
#include "netkit/tcp_server.h"
#include "netkit/retcode.h"
#include "logger/logger.h"
#include <sys/epoll.h>

namespace netkit { namespace epoll {

class ConnectionManager final {
public:
    ConnectionManager(Logger* logger) : m_event_idx(0), m_nr_valid_event(0), m_logger(logger) {}
    ~ConnectionManager();

    RetCode Init();

    /** @brief creates a tcp server, register to io_uring with `tag` and returns the server */
    TcpServer CreateTcpServer(const char* addr, uint16_t port, void* tag);
    RetCode CreateTcpClient(const char* addr, uint16_t port, Connection*);

    /** @brief usually called in the server side after new clients are accepted. */
    RetCode InitializeConnection(int client_fd, Connection*);

    /**
       @param `res` has different meanings according to events:
         - CONNECTED: `res` is the client fd, `tag` is the value passed to `CreateTcpServer()`
         - SEND: `res` is the number of bytes sent, `tag` is the value passed to `Connection::WriteAsync()`.
         - RECV: `res` is the number of bytes received, `tag` is the value passed to `Connection::ReadAsync()`.
         - SHUTDOWN: `res` is unused, `tag` is the value passed to `Connection::ShutDownAsync()`.

       @note also note that callers should handle client states when `res` is non-positive.
    */
    RetCode Wait(int64_t* res, void** tag);

private:
    static constexpr uint32_t MAX_EVENTS = 64;

private:
    int m_epfd;
    int m_event_idx;
    int m_nr_valid_event;
    struct epoll_event m_event_list[MAX_EVENTS];
    Logger* m_logger;

private:
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager(ConnectionManager&&) = delete;
    void operator=(const ConnectionManager&) = delete;
    void operator=(ConnectionManager&&) = delete;
};

}}

#endif
