#include "misc.h"
#include "internal_sender.h"
#include <string.h> // strerror()
using namespace std;

namespace netkit {

int InternalSender::DoWrite(const void* buf, uint64_t sz, EventQueue* eq) {
loop:
    int err =
        eq->WriteAsync(m_conn->fd, buf, sz, static_cast<EventHandler*>(this));
    if (ShouldRetry(err)) {
        goto loop;
    }

    if (err) {
        logger_error(m_logger, "writing data failed: [%s].", strerror(-err));
        // fall through
    }

    return err;
}

int InternalSender::Start(EventQueue* eq) {
    SendItem* item;
    {
        lock_guard<mutex> _l(m_conn->send_lock);
        item = &m_conn->send_queue.front();
    }

    return DoWrite(item->data.data(), item->data.size(), eq);
}

bool InternalSender::Process(EventResult res, EventQueue* eq) {
    SendItem* item;
    {
        lock_guard<mutex> _l(m_conn->send_lock);
        item = &m_conn->send_queue.front();
    }

    if (res.val < 0) {
        logger_error(m_logger, "send data failed: [%s].", strerror(-res.val));
        item->on_sent(res.val);
        m_conn->ShutDown(m_logger);
        return false;
    }
    if (res.val == 0) {
        logger_info(m_logger, "peer disconnected.");
        return false;
    }

    m_conn->send_offset += static_cast<uint32_t>(res.val);
    if (m_conn->send_offset == item->data.size()) {
        item->on_sent(0);
        m_conn->send_offset = 0;
        lock_guard<mutex> _l(m_conn->send_lock);
        m_conn->send_queue.pop();
        if (m_conn->send_queue.empty()) {
            return false;
        }
        item = &m_conn->send_queue.front();
    }

    int err = DoWrite(item->data.data() + m_conn->send_offset,
                      item->data.size() - m_conn->send_offset, eq);
    return (err == 0);
}

}
