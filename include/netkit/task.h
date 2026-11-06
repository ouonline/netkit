#ifndef __NETKIT_TASK_H__
#define __NETKIT_TASK_H__

#include "client_context.h"
#include "event_handler_ptr.h"

namespace netkit {

class Task : public EventHandler {
protected:
    Task(Logger* l) : m_logger(l) {}
    virtual ~Task() = default;

    virtual void Execute(ClientContext*) = 0;

protected:
    Logger* m_logger;

private:
    friend class ScheduleContext;

    void Init(const std::shared_ptr<Connection>& c) {
        m_conn = c;
    }

    bool Process(EventResult, EventQueue*) final;

private:
    std::shared_ptr<Connection> m_conn;
};

using TaskPtr = EventHandlerPtr<Task>;

}

#endif
