#include "emit_impl.h"
#include "internal_sender.h"
using namespace std;

namespace netkit {

static void DummyCallback(int) {}

void EmitImpl(Buffer&& data, const shared_ptr<Connection>& conn,
              EventQueue* curr_eq, EventQueue* target_eq,
              const function<void(int err)>& on_complete, Logger* logger) {
    if (data.IsEmpty()) {
        if (on_complete) {
            on_complete(0);
        }
        return;
    }

    bool is_empty_before_adding;
    {
        lock_guard<mutex> _l(conn->send_lock);
        is_empty_before_adding = conn->send_queue.empty();
        conn->send_queue.emplace(move(data), (on_complete) ?: DummyCallback);
    }

    if (is_empty_before_adding) {
        auto sender = new (nothrow) InternalSender(conn, logger);
        if (!sender) {
            logger_error(logger, "allocate sender failed: [%s].",
                         strerror(ENOMEM));
            conn->ShutDown(logger);
            conn->ClearSendQueue(-ENOMEM);
            return;
        }

        int err = sender->Start(curr_eq, target_eq);
        if (err) {
            logger_error(logger, "about to send data failed: [%s].",
                         strerror(-err));
            sender->DeleteSelf();
        }
    }
}

}
