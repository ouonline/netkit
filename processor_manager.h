#ifndef __TCP_PROCESSOR_MANAGER_H__
#define __TCP_PROCESSOR_MANAGER_H__

#include "deps/threadpool/cpp/thread_pool.hpp"
#include "status_code.h"
#include "processor_factory.h"
#include <stdint.h>

namespace utils { namespace net { namespace tcp {

class ProcessorManager final {

public:
    virtual ~ProcessorManager() {}
    StatusCode Init();
    StatusCode AddServer(const char* addr, uint16_t port,
                         const std::shared_ptr<ProcessorFactory>& factory);
    StatusCode AddClient(const char* addr, uint16_t port,
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
    int CreateClientFd(const char* host, uint16_t port);
    StatusCode SetNonBlocking(int fd);

private:
    int m_epfd;
    ThreadPool m_thread_pool;
};

}}}

#endif
