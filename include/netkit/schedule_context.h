#ifndef __NETKIT_SCHEDULE_CONTEXT_H__
#define __NETKIT_SCHEDULE_CONTEXT_H__

#include "task.h"
#include "scheduler.h"

namespace netkit {

class ScheduleContext final {
public:
    ScheduleContext(Scheduler* s, EventQueue* eq,
                    const std::shared_ptr<Connection>& c,
                    Logger* l)
        : m_sched(s), m_eq(eq), m_conn(c), m_logger(l) {}

    uint32_t GetWorkerNum() const {
        return m_sched->GetWorkerNum();
    }

    int Schedule(TaskPtr);
    int Deliver(TaskPtr, uint32_t idx);

private:
    Scheduler* m_sched;
    EventQueue* m_eq;
    std::shared_ptr<Connection> m_conn;
    Logger* m_logger;
};

}

#endif
