#include "netkit/connection.h"
#include "internal_utils.h"
using namespace std;

namespace netkit {

int Connection::AddTimer(const TimeVal& delay, const TimeVal& interval,
                         const function<void(int32_t val, Buffer* out)>& f) {
    return utils::AddTimer(delay, interval, f, m_new_rd_nq,
                           (InternalClient*)m_client_ptr, m_logger);
}

}
