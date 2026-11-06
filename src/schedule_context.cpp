#include "netkit/schedule_context.h"
#include <string.h> // strerror()
using namespace std;

namespace netkit {

int ScheduleContext::Schedule(TaskPtr ptr) {
    Task* task = ptr.release();
    task->Init(m_conn);

    int err = m_sched->Schedule(0, static_cast<EventHandler*>(task), m_eq);
    if (err) {
        logger_error(m_logger, "assign task to worker thread failed: [%s].",
                     strerror(-err));
        task->DeleteSelf();
        return err;
    }

    return 0;
}

int ScheduleContext::Deliver(TaskPtr ptr, uint32_t idx) {
    Task* task = ptr.release();
    task->Init(m_conn);

    int err = m_sched->Deliver(0, static_cast<EventHandler*>(task), m_eq, idx);
    if (err) {
        logger_error(m_logger, "assign task to worker thread failed: [%s].",
                     strerror(-err));
        task->DeleteSelf();
        return err;
    }

    return 0;
}

}
