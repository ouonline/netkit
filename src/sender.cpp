#include "misc.h"
#include "sender.h"
#include <string.h> // strerror()
#include <sys/socket.h> // shutdown()
using namespace std;

namespace netkit {

int Sender::Start(NotificationQueue* nq) {
    SendItem* item;
    {
        lock_guard<mutex> _l(m_conn->lock);
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

bool Sender::Process(int64_t res, NotificationQueue* nq) {
    SendItem* item;
    {
        lock_guard<mutex> _l(m_conn->lock);
        item = &m_conn->send_queue.front();
    }

    if (res < 0) {
        logger_error(m_conn->logger, "send data failed: [%s].", strerror(-res));
        item->on_complete(res);
        shutdown(m_conn->fd, SHUT_RDWR);
        return false;
    }
    if (res == 0) {
        logger_info(m_conn->logger, "peer disconnected.");
        return false;
    }

    m_conn->sending_offset += res;
    if (m_conn->sending_offset == item->data.size()) {
        item->on_complete(0);
        m_conn->sending_offset = 0;
        lock_guard<mutex> _l(m_conn->lock);
        m_conn->send_queue.pop();
        if (m_conn->send_queue.empty()) {
            return false;
        }
        item = &m_conn->send_queue.front();
    }

    if (m_conn->fd < 0) {
        return false;
    }

    do {
        res = nq->WriteAsync(m_conn->fd,
                             item->data.data() + m_conn->sending_offset,
                             item->data.size() - m_conn->sending_offset,
                             static_cast<EventHandler*>(this));
    } while (ShouldRetry(res));
    if (res) {
        logger_error(m_conn->logger, "sending data failed: [%s].",
                     strerror(-res));
        return false;
    }

    return true;
}

}
