#include "misc.h"
#include "internal_timer.h"
#include <string.h> // strerror()
#include <unistd.h> // close()
using namespace std;

namespace netkit {

InternalTimer::~InternalTimer() {
    close(m_fd);
}

int InternalTimer::Start(NotificationQueue* nq) {
    int err;
    do {
        err = nq->ReadAsync(m_fd, &m_nr_expiration, sizeof(m_nr_expiration),
                            static_cast<EventHandler*>(this));
    } while (ShouldRetry(err));
    if (err) {
        logger_error(m_logger, "launch read operation failed: [%s].",
                     strerror(-err));
        return err;
    }

    return 0;
}

bool InternalTimer::Process(EventResult res, NotificationQueue* nq) {
    if (res.err) {
        logger_error(m_logger, "read timer expirations failed: [%s].",
                     strerror(res.err));
        m_cb(-res.err);
        return false;
    }
    if (res.val == 0) {
        return false;
    }

    m_cb(m_nr_expiration);

    int rc;
    do {
        rc = nq->ReadAsync(m_fd, &m_nr_expiration, sizeof(m_nr_expiration),
                           static_cast<EventHandler*>(this));
    } while (ShouldRetry(rc));
    if (rc) {
        logger_error(m_logger, "launch read operation failed: [%s].",
                     strerror(-rc));
        m_cb(rc);
        return false;
    }

    return true;
}

}
