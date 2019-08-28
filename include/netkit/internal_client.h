#ifndef __TCP_INTERNAL_CLIENT_H__
#define __TCP_INTERNAL_CLIENT_H__

#include "processor.h"
#include "processor_factory.h"
#include "event_handler.h"
#include "buffer.h"
#include "threadkit/threadpool.h"
#include "logger/logger.h"
#include <memory>

namespace outils { namespace net { namespace tcp {

class InternalClient final : public EventHandler {
public:
    InternalClient(int fd, const std::shared_ptr<ProcessorFactory>& factory, ThreadPool* tp, Logger*);

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
    Processor* CreateProcessor();

private:
    int m_fd;
    uint32_t m_bytes_needed;
    Logger* m_logger;
    ThreadPool* m_tp;
    Processor* m_processor;
    std::shared_ptr<ProcessorFactory> m_factory;
    Connection m_conn;

private:
    InternalClient(const InternalClient&);
    InternalClient& operator=(const InternalClient&);
};

}}} // namespace outils::net::tcp

#endif
