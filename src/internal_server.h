#ifndef __NETKIT_INTERNAL_SERVER_H__
#define __NETKIT_INTERNAL_SERVER_H__

#include "netkit/event_handler.h"
#include "netkit/event_manager.h"
#include "netkit/processor_factory.h"
#include "netkit/status_code.h"
#include "threadkit/threadpool.h"
#include <string>
#include <memory>

namespace netkit { namespace tcp {

class InternalServer final : public EventHandler {
public:
    InternalServer(int fd, const std::shared_ptr<ProcessorFactory>& factory, EventManager* event_mgr,
                   threadkit::ThreadPool* tp, Logger* logger)
        : m_fd(fd), m_logger(logger), m_event_mgr(event_mgr), m_factory(factory), m_tp(tp) {}

    int GetFd() const override {
        return m_fd;
    }
    StatusCode In() override;
    StatusCode Out() override {
        return SC_OK;
    }
    void Error() override {}

private:
    int m_fd;
    Logger* m_logger;
    EventManager* m_event_mgr;
    std::shared_ptr<ProcessorFactory> m_factory;
    threadkit::ThreadPool* m_tp;

private:
    InternalServer(const InternalServer&);
    InternalServer& operator=(const InternalServer&);
};

}} // namespace netkit::tcp

#endif
