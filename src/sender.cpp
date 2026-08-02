#include "misc.h"
#include "sender.h"
#include <string.h> // strerror()
using namespace std;

namespace netkit {

int Sender::Start(NotificationQueue* nq) {
    SendItem* item;
    {
        lock_guard<mutex> _l(m_conn->send_lock);
        item = &m_conn->send_queue.front();
    }

    int err;
    do {
        err = nq->WriteAsync(m_conn->fd, item->data.data(), item->data.size(),
                             static_cast<EventHandler*>(this));
    } while (ShouldRetry(err));
    if (err) {
        logger_error(m_conn->logger, "sending data failed: [%s].",
                     strerror(-err));
        // fall through
    }

    return err;
}

bool Sender::Process(EventResult res, NotificationQueue* nq) {
    SendItem* item;
    {
        lock_guard<mutex> _l(m_conn->send_lock);
        item = &m_conn->send_queue.front();
    }

    if (res.err) {
        logger_error(m_conn->logger, "send data failed: [%s].",
                     strerror(res.err));
        item->on_complete(-res.err);
        m_conn->ShutDown();
        return false;
    }
    if (res.val == 0) {
        logger_info(m_conn->logger, "peer disconnected.");
        return false;
    }

    m_conn->send_offset += res.val;
    if (m_conn->send_offset == item->data.size()) {
        item->on_complete(0);
        m_conn->send_offset = 0;
        lock_guard<mutex> _l(m_conn->send_lock);
        m_conn->send_queue.pop();
        if (m_conn->send_queue.empty()) {
            return false;
        }
        item = &m_conn->send_queue.front();
    }

    int rc;
    do {
        rc = nq->WriteAsync(m_conn->fd, item->data.data() + m_conn->send_offset,
                            item->data.size() - m_conn->send_offset,
                            static_cast<EventHandler*>(this));
    } while (ShouldRetry(rc));
    if (rc) {
        logger_error(m_conn->logger, "sending data failed: [%s].",
                     strerror(-rc));
        return false;
    }

    return true;
}

}
