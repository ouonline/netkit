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

class ProcessorTask final : public threadkit::ThreadTask {
public:
    ProcessorTask(const std::shared_ptr<Processor>& p, Connection* c) : m_conn(c), m_processor(p) {}
    Buffer* GetBuffer() {
        return &m_buf;
    }
    std::shared_ptr<threadkit::ThreadTask> Run() override {
        m_processor->ProcessPacket(&m_buf, m_conn);
        return std::shared_ptr<threadkit::ThreadTask>();
    }

private:
    Connection* m_conn;
    std::shared_ptr<Processor> m_processor;
    Buffer m_buf;
};

class InternalClient final : public EventHandler {
public:
    InternalClient(int fd, const std::shared_ptr<Processor>& p, threadkit::ThreadPool* tp, Logger*);

    int GetFd() const override {
        return m_fd;
    }
    RetCode In() override;
    RetCode Out() override {
        return RC_NOT_IMPLEMENTED;
    }
    void ShutDown() override;

private:
    int m_fd;
    Connection m_conn;
    std::shared_ptr<ProcessorTask> m_task;
    std::shared_ptr<Processor> m_processor;

    // ----- shared data ----- //

    Logger* m_logger;
    threadkit::ThreadPool* m_tp;

private:
    InternalClient(const InternalClient&);
    InternalClient& operator=(const InternalClient&);
};

}} // namespace netkit::tcp

#endif
