#ifndef __NETKIT_CONNECTION_H__
#define __NETKIT_CONNECTION_H__

#include "netkit/nq_utils.h"
#include "netkit/buffer.h"
#include "connection_info.h"
#include "utils.h"
#include <functional>

namespace netkit {

class InternalClient;

class Connection final {
private:
    static void DummySentCallback(int) {}

public:
    Connection(int fd, NotificationQueueImpl* new_rd_nq, NotificationQueueImpl* wr_nq,
               InternalClient* client, Logger* l)
        : m_new_rd_nq(new_rd_nq), m_wr_nq(wr_nq), m_client(client), m_logger(l) {
        utils::GenConnectionInfo(fd, &m_info);
    }

    const ConnectionInfo& info() const {
        return m_info;
    }

    /** returns -errno or fd of the timer */
    int AddTimer(const TimeVal& delay, const TimeVal& interval,
                 /*
                   `val` < 0: error occurs and `val` == -errno
                   `val` > 0: the number of expirations
                 */
                 const std::function<int(int32_t val)>&);

    int SendAsync(Buffer&&, const std::function<void(int err)>& = DummySentCallback);

private:
    ConnectionInfo m_info;
    NotificationQueueImpl* m_new_rd_nq;
    NotificationQueueImpl* m_wr_nq;
    InternalClient* m_client;
    Logger* m_logger;
};

}

#endif
