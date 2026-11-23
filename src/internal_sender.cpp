#include "misc.h"
#include "internal_sender.h"
#include <string.h> // strerror()
#include <limits>
using namespace std;

namespace netkit {

int InternalSender::DoWrite(const void* buf, uint64_t sz, EventQueue* eq) {
    int err;

    while (true) {
        err = eq->WriteAsync(m_conn->fd, buf, sz,
                             static_cast<EventHandler*>(this));
        if (!ShouldRetry(err)) {
            break;
        }
    }

    if (err) {
        logger_error(m_logger, "send data failed: [%s].", strerror(-err));
        m_conn->ShutDown(m_logger);
        m_conn->ClearSendQueue(err);
    }

    return err;
}

int InternalSender::Start(EventQueue* curr, EventQueue* dst) {
    if (dst) {
        int err = curr->NotifyAsync(dst, numeric_limits<int32_t>::max(),
                                    static_cast<EventHandler*>(this));
        if (err) {
            logger_error(m_logger, "send data failed: [%s].", strerror(-err));
            m_conn->ShutDown(m_logger);
            m_conn->ClearSendQueue(err);
        }
        return err;
    }

    SendItem* item;
    {
        lock_guard<mutex> _l(m_conn->send_lock);
        item = &m_conn->send_queue.front();
    }

    return DoWrite(item->buffer.data(), item->buffer.size(), curr);
}

bool InternalSender::Process(EventResult res, EventQueue* eq) {
    SendItem* item;
    {
        lock_guard<mutex> _l(m_conn->send_lock);
        item = &m_conn->send_queue.front();
    }

    if (res.val < 0) {
        logger_error(m_logger, "send data failed: [%s].", strerror(-res.val));
        m_conn->ShutDown(m_logger);
        m_conn->ClearSendQueue(res.val);
        return false;
    }
    if (res.val == 0) {
        logger_info(m_logger, "peer disconnected.");
        m_conn->ShutDown(m_logger);
        m_conn->ClearSendQueue(-ECONNRESET);
        return false;
    }

    // special value for notification
    if (res.val == numeric_limits<decltype(res.val)>::max()) {
        res.val = 0;
    }

    m_conn->send_offset += static_cast<uint32_t>(res.val);
    if (m_conn->send_offset == item->buffer.size()) {
        item->on_complete(0);
        m_conn->send_offset = 0;
        lock_guard<mutex> _l(m_conn->send_lock);
        m_conn->send_queue.pop();
        if (m_conn->send_queue.empty()) {
            return false;
        }
        item = &m_conn->send_queue.front();
    }

    int err = DoWrite(item->buffer.data() + m_conn->send_offset,
                      item->buffer.size() - m_conn->send_offset, eq);
    return (err == 0);
}

}
