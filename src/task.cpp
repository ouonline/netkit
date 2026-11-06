#include "netkit/task.h"
#include <string.h> // strerror()

namespace netkit {

bool Task::Process(EventResult res, EventQueue* eq) {
    // this task is returned to the sender's queue because it cannot be
    // delivered to the worker. see `EventQueue::NotifyAsync()`.
    if (res.val < 0) {
        logger_error(m_logger, "assign task to worker thread failed: [%s].",
                     strerror(-res.val));
        // closes the connection as if `TcpClient::OnPacketReady()` failed
        m_conn->ShutDown(m_logger);
        return false;
    }

    ClientContext ctx(m_conn, eq, m_logger);
    Execute(&ctx);
    return false;
}

}
