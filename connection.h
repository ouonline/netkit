#ifndef __NET_CONNECTION_H__
#define __NET_CONNECTION_H__

#include "status_code.h"
#include <string>
#include <stdint.h>
#include <pthread.h>

namespace utils { namespace net {

struct ConnectionInfo {
    uint16_t local_port = 0;
    uint16_t remote_port = 0;
    std::string local_addr;
    std::string remote_addr;
};

class Connection final {

public:
    Connection(int fd);
    ~Connection();
    int Send(const char* data, int size, uint32_t timeout_ms = 0);
    const ConnectionInfo& GetConnectionInfo() const { return m_info; }

private:
    StatusCode RealSetSendTimeout(uint32_t ms);

private:
    int m_fd;
    ConnectionInfo m_info;
    pthread_mutex_t m_lock;

private:
    Connection(const Connection&);
    Connection& operator=(const Connection&);
};

}}

#endif
