#ifndef __TCP_INTERNAL_SERVER_H__
#define __TCP_INTERNAL_SERVER_H__

#include "event_handler.h"
#include "event_manager.h"
#include "processor_factory.h"
#include "status_code.h"
#include "deps/threadpool/cpp/thread_pool.hpp"
#include <string>

namespace utils { namespace net { namespace tcp {

class InternalServer final : public EventHandler {

public:
    InternalServer(int fd, const std::shared_ptr<ProcessorFactory>& factory,
                   EventManager* event_mgr, ThreadPool* tp)
        : m_fd(fd), m_event_mgr(event_mgr), m_factory(factory), m_tp(tp) {}

    int GetFd() const override { return m_fd; }
    StatusCode In() override;
    StatusCode Out() override { return SC_OK; }
    void Error() override {}

private:
    int m_fd;
    EventManager* m_event_mgr;
    std::shared_ptr<ProcessorFactory> m_factory;
    ThreadPool* m_tp;
};

}}}

#endif
