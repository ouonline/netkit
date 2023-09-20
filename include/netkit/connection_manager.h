#ifndef __NETKIT_CONNECTION_MANAGER_H__
#define __NETKIT_CONNECTION_MANAGER_H__

#include "retcode.h"
#include "liburing.h"
#include "connection.h"
#include "logger/logger.h"
#include <stdint.h>
#include <memory>

namespace netkit {

class ConnectionManager final {
public:
    ConnectionManager(Logger* logger) : m_logger(logger) {}
    ~ConnectionManager();

    RetCode Init();

    RetCode CreateTcpServer(const char* addr, uint16_t port,
                            const std::function<void(const std::shared_ptr<Connection>&)>& cb);
    std::shared_ptr<Connection> CreateTcpClient(const char* addr, uint16_t port);

    RetCode Run();

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
