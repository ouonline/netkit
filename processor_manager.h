#ifndef __TCP_PROCESSOR_MANAGER_H__
#define __TCP_PROCESSOR_MANAGER_H__

#include "threadpool/cpp/thread_pool.hpp"
#include "status_code.h"
#include "processor_factory.h"
#include "event_manager.h"
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
    StatusCode SetReuseAddr(int fd);
    StatusCode GetHostInfo(const char* addr, uint16_t port,
                           struct addrinfo** svr);
    int CreateServerFd(const char* addr, uint16_t port);
    int CreateClientFd(const char* host, uint16_t port);

private:
    EventManager m_event_mgr;
    ThreadPool m_thread_pool;
};

}}}

#endif
