#ifndef __NETKIT_CONNECTION_H__
#define __NETKIT_CONNECTION_H__

#include "retcode.h"
#include "logger/logger.h"
#include <string>
#include <pthread.h>
#include <stdint.h>

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
    Connection(int fd, Logger* logger);
    ~Connection();
    RetCode SetSendTimeout(uint32_t ms);
    RetCode Send(const void* data, uint64_t size, uint64_t* bytes_left = nullptr);
    const Info& GetInfo() const {
        return m_info;
    }

private:
    int m_fd;
    Info m_info;
    pthread_mutex_t m_lock;
    Logger* m_logger;

private:
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
};

} // namespace netkit

#endif
