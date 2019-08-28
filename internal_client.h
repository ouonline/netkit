#ifndef __TCP_INTERNAL_CLIENT_H__
#define __TCP_INTERNAL_CLIENT_H__

#include "processor.h"
#include "processor_factory.h"
#include "event_handler.h"
#include "buffer.h"
#include "deps/threadpool/cpp/thread_pool.hpp"
#include <memory>

namespace utils { namespace net { namespace tcp {

class InternalClient final : public EventHandler {

public:
    InternalClient(int fd, const std::shared_ptr<ProcessorFactory>& factory, ThreadPool* tp);
    virtual ~InternalClient() {}

    int GetFd() const override { return m_fd; }
    StatusCode In() override;
    StatusCode Out() override { return SC_OK; }
    void Error() override;

private:
    StatusCode ReadData();
    std::shared_ptr<Processor> CreateProcessor();

private:
    int m_fd;
    uint32_t m_bytes_needed;
    ThreadPool* m_tp;
    Connection m_conn;
    std::shared_ptr<ProcessorFactory> m_factory;
    std::shared_ptr<Processor> m_processor;
};

}}}

#endif
