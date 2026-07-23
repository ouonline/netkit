#ifndef __NETKIT_SEND_CONTEXT_H__
#define __NETKIT_SEND_CONTEXT_H__

#include "utils.h"
#include "connection.h"
#include "notification_queue.h"
#include <memory>
#include <functional>

namespace netkit {

class SendContext final {
public:
    SendContext(const std::shared_ptr<Connection>& c, NotificationQueue* nq)
        : m_conn(c), m_nq(nq) {}

    const EndpointInfo& GetEndpointInfo() {
        return m_conn->GetEndpointInfo();
    }

    int Emit(Buffer&&, const std::function<void(int err)>& on_complete = {});

private:
    const std::shared_ptr<Connection>& m_conn;
    NotificationQueue* m_nq;
};

}

#endif
