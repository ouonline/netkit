#ifndef __NETKIT_PROCESSOR_MANAGER_H__
#define __NETKIT_PROCESSOR_MANAGER_H__

#include "threadkit/threadpool.h"
#include "status_code.h"
#include "processor_factory.h"
#include "event_manager.h"
#include "logger/logger.h"
#include <stdint.h>
#include <memory>

namespace netkit { namespace tcp {

class ProcessorManager final {
public:
    ProcessorManager(Logger* logger) : m_logger(logger), m_event_mgr(logger) {}
    virtual ~ProcessorManager() {}
    StatusCode Init();
    StatusCode AddServer(const char* addr, uint16_t port, const std::shared_ptr<ProcessorFactory>& factory);
    StatusCode AddClient(const char* addr, uint16_t port, const std::shared_ptr<ProcessorFactory>& factory);
    StatusCode Run();

private:
    StatusCode SetReuseAddr(int fd);
    StatusCode GetHostInfo(const char* addr, uint16_t port, struct addrinfo** svr);
    int CreateServerFd(const char* addr, uint16_t port);
    int CreateClientFd(const char* host, uint16_t port);

private:
    Logger* m_logger;
    EventManager m_event_mgr;
    threadkit::ThreadPool m_thread_pool;

private:
    ProcessorManager(const ProcessorManager&);
    ProcessorManager& operator=(const ProcessorManager&);
};

}} // namespace netkit::tcp

#endif
