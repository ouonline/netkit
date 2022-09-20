#ifndef __NETKIT_INTERNAL_CLIENT_H__
#define __NETKIT_INTERNAL_CLIENT_H__

#include "netkit/processor.h"
#include "netkit/processor_factory.h"
#include "netkit/event_handler.h"
#include "netkit/buffer.h"
#include "threadkit/threadpool.h"
#include "logger/logger.h"
#include <memory>

namespace netkit { namespace tcp {

class InternalClient final : public EventHandler {
public:
    InternalClient(int fd, const std::shared_ptr<ProcessorFactory>& factory, threadkit::ThreadPool* tp, Logger*);

    int GetFd() const override {
        return m_fd;
    }
    StatusCode In() override;
    StatusCode Out() override {
        return SC_OK;
    }
    void Error() override;

private:
    StatusCode ReadData();

private:
    int m_fd;
    uint32_t m_bytes_needed;
    Connection m_conn;
    std::shared_ptr<Processor> m_processor;

    // ----- shared data ----- //

    Logger* m_logger;
    threadkit::ThreadPool* m_tp;
    std::shared_ptr<ProcessorFactory> m_factory;

private:
    InternalClient(const InternalClient&);
    InternalClient& operator=(const InternalClient&);
};

}} // namespace netkit::tcp

#endif
