#ifndef __NET_CONNECTION_H__
#define __NET_CONNECTION_H__

#include "status_code.h"
#include "logger/logger.h"
#include <string>
#include <stdint.h>
#include <pthread.h>

namespace outils { namespace net {

struct ConnectionInfo {
    uint16_t local_port = 0;
    uint16_t remote_port = 0;
    std::string local_addr;
    std::string remote_addr;
};

class Connection final {

public:
    Connection(int fd, struct logger* logger);
    ~Connection();
    void SetSendTimeout(uint32_t ms);
    int Send(const void* data, uint32_t size);
    const ConnectionInfo& GetConnectionInfo() const { return m_info; }

private:
    StatusCode RealSetSendTimeout(uint32_t ms);

private:
    int m_fd;
    uint32_t m_send_timeout;
    struct logger* m_logger;
    ConnectionInfo m_info;
    pthread_mutex_t m_lock;

private:
    Connection(const Connection&);
    Connection& operator=(const Connection&);
};

}}

#endif
