#include "netkit/send_context.h"
#include "sender.h"
#include <string.h> // strerror()
using namespace std;

namespace netkit {

static void DummyCallback(int) {}

int SendContext::Emit(Buffer&& b, const function<void(int err)>& on_complete) {
    bool is_empty_before_adding;
    {
        lock_guard<mutex> _l(m_conn->lock);
        is_empty_before_adding = m_conn->send_queue.empty();
        m_conn->send_queue.emplace(move(b), (on_complete) ?: DummyCallback);
    }

    if (is_empty_before_adding) {
        auto sender = new Sender(m_conn);
        if (!sender) {
            logger_error(m_conn->logger, "allocate sender failed: [%s].",
                         strerror(ENOMEM));
            return -ENOMEM;
        }

        int err = sender->Start(m_nq);
        if (err) {
            logger_error(m_conn->logger, "about to send data failed: [%s].",
                         strerror(-err));
            sender->DeleteSelf();
            return err;
        }
    }

    return 0;
}

}
