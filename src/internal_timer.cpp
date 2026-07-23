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

bool InternalTimer::Process(int64_t res, NotificationQueue* nq) {
    if (res < 0) {
        logger_error(m_logger, "read timer expirations failed: [%s].",
                     strerror(-res));
        m_cb(res);
        return false;
    }
    if (res == 0) {
        return false;
    }

    m_cb(m_nr_expiration);

    do {
        res = nq->ReadAsync(m_fd, &m_nr_expiration, sizeof(m_nr_expiration),
                            static_cast<EventHandler*>(this));
    } while (ShouldRetry(res));
    if (res) {
        logger_error(m_logger, "launch read operation failed: [%s].",
                     strerror(-res));
        m_cb(res);
        return false;
    }

    return true;
}

}
