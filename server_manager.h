#ifndef __TCP_SERVER_MANAGER_H__
#define __TCP_SERVER_MANAGER_H__

#include "deps/threadpool/cpp/thread_pool.hpp"
#include "status_code.h"
#include "processor_factory.h"
#include "server_manager.h"
#include <stdint.h>

namespace utils { namespace net { namespace tcp {

class ServerManager final {

public:
    virtual ~ServerManager() {}
    StatusCode Init();
    StatusCode AddServer(const char* addr, uint16_t port,
                         const std::shared_ptr<ProcessorFactory>& factory);
    StatusCode Run();

private:
    StatusCode GetHostInfo(const char* addr, uint16_t port,
                           struct addrinfo** svr);
    void Time2Timeval(uint32_t ms, struct timeval* t);
    StatusCode SetSendTimeout(int fd, uint32_t ms);
    StatusCode SetRecvTimeout(int fd, uint32_t ms);
    StatusCode SetReuseAddr(int fd);
    int CreateServerFd(const char* addr, uint16_t port);

private:
    int m_epfd;
    ThreadPool m_thread_pool;
};

}}}

#endif
