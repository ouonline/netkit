#include "netkit/client_context.h"
#include "netkit/timer.h"
#include "netkit/utils.h"
#include "internal_sender.h"
#include <string.h> // strerror()
using namespace std;

namespace netkit {

static void DummyCallback(int) {}

void ClientContext::EmitImpl(Buffer&& data, EventQueue* target_eq,
                             const function<void(int err)>& on_complete) {
    if (data.IsEmpty()) {
        if (on_complete) {
            on_complete(0);
        }
        return;
    }

    bool is_empty_before_adding;
    {
        lock_guard<mutex> _l(m_conn->send_lock);
        is_empty_before_adding = m_conn->send_queue.empty();
        m_conn->send_queue.emplace(move(data), (on_complete) ?: DummyCallback);
    }

    if (is_empty_before_adding) {
        auto sender = new (nothrow) InternalSender(m_conn, m_logger);
        if (!sender) {
            logger_error(m_logger, "allocate sender failed: [%s].",
                         strerror(ENOMEM));
            m_conn->ShutDown(m_logger);
            m_conn->ClearSendQueue(-ENOMEM);
            return;
        }

        int err = sender->Start(m_eq, target_eq);
        if (err) {
            logger_error(m_logger, "about to send data failed: [%s].",
                         strerror(-err));
            sender->DeleteSelf();
        }
    }
}

int ClientContext::AddTimer(const TimeVal& interval, TimerPtr ptr) {
    if (!ptr) {
        return -EINVAL;
    }

    int fd = utils::CreateTimerFd(interval, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "CreateTimerFd failed: [%s].", strerror(-fd));
        return fd;
    }

    Timer* timer = ptr.release();
    int err = timer->Init(fd, m_conn);
    if (err) {
        logger_error(m_logger, "init timer failed: [%s].", strerror(-err));
        timer->DeleteSelf();
        return err;
    }

    err = timer->Start(m_eq);
    if (err) {
        logger_error(m_logger, "start timer failed: [%s].", strerror(-err));
        timer->DeleteSelf();
        return err;
    }

    return fd;
}

}
