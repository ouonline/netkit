#include "emit_impl.h"
#include "netkit/client_context.h"
#include "netkit/timer.h"
#include "netkit/utils.h"
#include <string.h> // strerror()
using namespace std;

namespace netkit {

void ClientContext::Emit(Buffer&& data,
                         const function<void(int err)>& on_complete) {
    EmitImpl(move(data), m_conn, m_eq, nullptr, on_complete, m_logger);
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
