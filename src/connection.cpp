#include "netkit/utils.h"
#include "netkit/connection.h"
#include <string.h> // strerror()
#include <unistd.h> // close()
#include <sys/timerfd.h> // timerfd_settime()
#include <sys/socket.h> // shutdown()
using namespace std;

namespace netkit {

Connection::~Connection() {
    if (fd >= 0) {
        close(fd);
    }
}

const EndpointInfo& Connection::GetEndpointInfo() {
    if (m_info.remote_port == 0) {
        lock_guard<mutex> _l(timer_lock); // use timer lock
        if (m_info.remote_port == 0) {
            utils::GenEndpointInfo(fd, &m_info);
        }
    }
    return m_info;
}

void Connection::ShutDown(Logger* logger) {
    if (!m_is_valid.exchange(0, memory_order_acq_rel)) {
        return;
    }

    shutdown(fd, SHUT_RDWR);

    const struct itimerspec ts = {{0, 0}, {0, 1}};
    lock_guard<mutex> _l(timer_lock);
    for (auto fd : timer_fds) {
        int err = timerfd_settime(fd, 0, &ts, nullptr);
        if (err) {
            logger_error(logger, "waking timer failed: [%s].", strerror(errno));
        }
    }
}

}
