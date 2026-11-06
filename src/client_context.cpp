#include "netkit/client_context.h"
#include "netkit/timer.h"
#include "netkit/utils.h"
#include "internal_sender.h"
#include <string.h> // strerror()
using namespace std;

namespace netkit {

static void DummyCallback(int) {}

int ClientContext::Emit(Buffer&& b, const function<void(int err)>& on_sent) {
    bool is_empty_before_adding;
    {
        lock_guard<mutex> _l(m_conn->send_lock);
        is_empty_before_adding = m_conn->send_queue.empty();
        m_conn->send_queue.emplace(move(b), (on_sent) ?: DummyCallback);
    }

    if (is_empty_before_adding) {
        auto sender = new InternalSender(m_conn, m_logger);
        if (!sender) {
            logger_error(m_logger, "allocate sender failed: [%s].",
                         strerror(ENOMEM));
            return -ENOMEM;
        }

        int err = sender->Start(m_eq);
        if (err) {
            logger_error(m_logger, "about to send data failed: [%s].",
                         strerror(-err));
            sender->DeleteSelf();
            return err;
        }
    }

    return 0;
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
