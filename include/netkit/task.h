#ifndef __NETKIT_TASK_H__
#define __NETKIT_TASK_H__

#include "send_context.h"
#include "event_handler_ptr.h"

namespace netkit {

class Task : public EventHandler {
protected:
    virtual ~Task() = default;

    virtual void Run(SendContext*) = 0;

    Logger* logger() const {
        return m_conn->logger;
    }

protected:
    Buffer m_buffer;

private:
    friend class TcpClient;

    void Init(Buffer&& b, const std::shared_ptr<Connection>& c) {
        m_buffer = std::move(b);
        m_conn = c;
    }

    bool Process(EventResult, NotificationQueue* nq) final {
        SendContext ctx(m_conn, nq);
        Run(&ctx);
        return false;
    }

private:
    std::shared_ptr<Connection> m_conn;
};

using TaskPtr = EventHandlerPtr<Task>;

}

#endif
