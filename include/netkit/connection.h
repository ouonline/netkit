#ifndef __NETKIT_CONNECTION_H__
#define __NETKIT_CONNECTION_H__

#include "retcode.h"
#include "logger/logger.h"
#include <pthread.h>
#include <string>
#include <stdint.h>

namespace netkit {

struct ConnectionInfo final {
    uint16_t local_port = 0;
    uint16_t remote_port = 0;
    std::string local_addr;
    std::string remote_addr;
};

class Connection final {
public:
    Connection(int fd, Logger* logger);
    ~Connection();
    RetCode SetSendTimeout(uint32_t ms);
    RetCode Send(const void* data, uint32_t  size, uint32_t* bytes_left = nullptr);
    const ConnectionInfo& GetConnectionInfo() const {
        return m_info;
    }

private:
    int m_fd;
    ConnectionInfo m_info;
    pthread_mutex_t m_lock;
    Logger* m_logger;

private:
    Connection(const Connection&);
    Connection& operator=(const Connection&);
};

} // namespace netkit

#endif
