#include "netkit/connection.h"
#include "internal_utils.h"
#include <cstring>
using namespace std;

namespace netkit {

int Connection::AddTimer(const TimeVal& delay, const TimeVal& interval,
                         const function<int(int32_t val)>& callback) {
    return utils::AddTimer(delay, interval, callback, m_new_rd_nq,
                           (InternalClient*)m_client_ptr, m_logger);
}

int Connection::SendAsync(Buffer&& buf) {
    int err = utils::InitThreadLocalNq(m_logger);
    if (err) {
        logger_error(m_logger, "init thread local logger failed: [%s].",
                     strerror(-err));
        return err;
    }

    auto session = CreateSession();
    if (!session) {
        logger_error(m_logger, "create Session failed: [%s].", strerror(ENOMEM));
        return -ENOMEM;
    }

    session->data = std::move(buf);
    session->client = static_cast<InternalClient*>(m_client_ptr);
    GetClient(session->client);

    err = utils::GetThreadLocalNq()->NotifyAsync(m_wr_nq, 0, session);
    if (err) {
        logger_error(m_logger, "NotifyAsync writing nq failed: [%s].", strerror(-err));
        PutClient(session->client);
        DestroySession(session);
    }
    return err;
}

}
