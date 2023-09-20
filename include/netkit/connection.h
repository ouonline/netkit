#ifndef __NETKIT_CONNECTION_H__
#define __NETKIT_CONNECTION_H__

#include "retcode.h"
#include "logger/logger.h"
#include "liburing.h"
#include <stdint.h>
#include <string>
#include <functional>

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
    Connection(int fd, struct io_uring* ring, Logger* logger);

    const Info& GetInfo() const {
        return m_info;
    }

    RetCode ReadAsync(void*, uint64_t, const std::function<void(uint64_t bytes_read)>& cb = {});
    RetCode WriteAsync(const void*, uint64_t, const std::function<void(uint64_t bytes_written)>& cb = {});

    RetCode ShutDownAsync(const std::function<void()>& cb = {});

private:
    int m_fd;
    struct io_uring* m_ring;
    Logger* m_logger;
    Info m_info;

private:
    Connection(const Connection&) = delete;
    Connection(Connection&&) = delete;
    void operator=(const Connection&) = delete;
    void operator=(Connection&&) = delete;
};

}

#endif
