#include "netkit/timer.h"
#include "netkit/send_context.h"
#include "misc.h"
#include <string.h> // strerror()
#include <unistd.h> // close()
using namespace std;

namespace netkit {

Timer::~Timer() {
    // m_conn is empty if CreateTimerFd() fails
    if (m_conn) {
        lock_guard<mutex> _l(m_conn->timer_lock);
        m_conn->timer_fds.erase(m_fd);
        close(m_fd);
    }
}

int Timer::Init(int fd, const shared_ptr<Connection>& conn) {
    m_fd = fd;
    m_conn = conn;

    lock_guard<mutex> _l(conn->timer_lock);
    if (!conn->IsValid()) {
        return -ENOTCONN;
    }

    conn->timer_fds.insert(fd);

    return 0;
}

int Timer::Start(NotificationQueue* nq) {
loop:
    int err = nq->ReadAsync(m_fd, &m_nr_expiration, sizeof(m_nr_expiration),
                            static_cast<EventHandler*>(this));
    if (ShouldRetry(err)) {
        goto loop;
    }

    if (err) {
        logger_error(m_conn->logger, "start timer failed: [%s].",
                     strerror(-err));
        // fall through
    }

    return err;
}

bool Timer::Process(EventResult res, NotificationQueue* nq) {
    if (!m_conn->IsValid()) {
        return false;
    }

    SendContext ctx(m_conn, nq);
    if (res.err) {
        logger_error(m_conn->logger, "read timer expirations failed: [%s].",
                     strerror(res.err));
        OnExpiration(-res.err, &ctx);
        return false;
    }

    bool keep = OnExpiration(m_nr_expiration, &ctx);
    if (!keep) {
        return false;
    }

    int err = Start(nq);
    if (err) {
        OnExpiration(err, &ctx);
        return false;
    }

    return true;
}

}
