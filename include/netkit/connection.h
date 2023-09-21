#ifndef __NETKIT_CONNECTION_H__
#define __NETKIT_CONNECTION_H__

#include "retcode.h"
#include "liburing.h"
#include "logger/logger.h"
#include <stdint.h>
#include <string>

namespace netkit {

class Connection final {
public:
    struct Info final {
        uint16_t local_port = 0;
        uint16_t remote_port = 0;
        std::string local_addr;
        std::string remote_addr;
    };

public:
    Connection() : m_fd(-1), m_ring(nullptr), m_logger(nullptr) {}
    Connection(Connection&&) = default;
    Connection& operator=(Connection&&) = default;

    const Info& GetInfo() const {
        return m_info;
    }

    RetCode RecvAsync(void* buf, uint64_t sz, void* tag);
    RetCode SendAsync(const void* buf, uint64_t sz, void* tag);
    RetCode ShutDownAsync(void* tag);

private:
    friend class ConnectionManager;
    void Init(int fd, struct io_uring* ring, Logger* logger);

private:
    int m_fd;
    struct io_uring* m_ring;
    Logger* m_logger;
    Info m_info;

private:
    Connection(const Connection&) = delete;
    void operator=(const Connection&) = delete;
};

}

#endif
