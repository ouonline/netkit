#ifndef __NETKIT_WRITER_H__
#define __NETKIT_WRITER_H__

#include "netkit/nq_utils.h"
#include "connection_info.h"
#include "buffer.h"
#include <functional>

namespace netkit {

class InternalClient;

class Writer final {
public:
    Writer(InternalClient* c, NotificationQueueImpl* wr_nq, NotificationQueueImpl* signal_nq)
        : m_client(c), m_wr_nq(wr_nq), m_signal_nq(signal_nq) {}

    const ConnectionInfo& GetConnectionInfo() const;
    int WriteAsync(Buffer&&, const std::function<void(int err)>& = {});

private:
    InternalClient* m_client;
    NotificationQueueImpl* m_wr_nq;
    NotificationQueueImpl* m_signal_nq;

private:
    Writer(Writer&&) = delete;
    Writer(const Writer&) = delete;
    void operator=(Writer&&) = delete;
    void operator=(const Writer&) = delete;
};

}

#endif
