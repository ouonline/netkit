#include "misc.h"
#include "sender.h"
#include <string.h> // strerror()
using namespace std;

namespace netkit {

int Sender::DoWrite(const void* buf, uint64_t sz, NotificationQueue* nq) {
loop:
    int err =
        nq->WriteAsync(m_conn->fd, buf, sz, static_cast<EventHandler*>(this));
    if (ShouldRetry(err)) {
        goto loop;
    }

    if (err) {
        logger_error(m_conn->logger, "writing data failed: [%s].",
                     strerror(-err));
        // fall through
    }

    return err;
}

int Sender::Start(NotificationQueue* nq) {
    SendItem* item;
    {
        lock_guard<mutex> _l(m_conn->send_lock);
        item = &m_conn->send_queue.front();
    }

    return DoWrite(item->data.data(), item->data.size(), nq);
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

    int err = DoWrite(item->data.data() + m_conn->send_offset,
                      item->data.size() - m_conn->send_offset, nq);
    return (err == 0);
}

}
