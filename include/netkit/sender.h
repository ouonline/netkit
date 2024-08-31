#ifndef __NETKIT_SENDER_H__
#define __NETKIT_SENDER_H__

#include "netkit/nq_utils.h"
#include "connection_info.h"
#include "buffer.h"
#include <functional>

namespace netkit {

class InternalClient;

class Sender final {
public:
    Sender(InternalClient* c, NotificationQueueImpl* wr_nq,
           NotificationQueueImpl* signal_nq)
        : m_client(c), m_wr_nq(wr_nq), m_signal_nq(signal_nq) {}

    const ConnectionInfo& GetConnectionInfo() const;
    int SendAsync(Buffer&&, const std::function<void(int err)>& callback = {});

private:
    InternalClient* m_client;
    NotificationQueueImpl* m_wr_nq;
    NotificationQueueImpl* m_signal_nq;

private:
    Sender(Sender&&) = delete;
    Sender(const Sender&) = delete;
    void operator=(Sender&&) = delete;
    void operator=(const Sender&) = delete;
};

}

#endif
