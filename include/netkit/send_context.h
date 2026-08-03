#ifndef __NETKIT_SEND_CONTEXT_H__
#define __NETKIT_SEND_CONTEXT_H__

#include "timer.h"
#include <functional>

namespace netkit {

class SendContext final {
public:
    SendContext(const std::shared_ptr<Connection>& c, NotificationQueue* nq)
        : m_conn(c), m_nq(nq) {}

    const EndpointInfo& GetEndpointInfo() {
        return m_conn->GetEndpointInfo();
    }

    // returns 0 or -errno
    int Emit(Buffer&&, const std::function<void(int err)>& on_complete = {});

    // returns -errno or timer fd
    int AddTimer(const TimeVal& delay, const TimeVal& interval, TimerPtr);

private:
    const std::shared_ptr<Connection>& m_conn;
    NotificationQueue* m_nq;
};

}

#endif
