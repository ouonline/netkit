#ifndef __NETKIT_CONNECTION_H__
#define __NETKIT_CONNECTION_H__

#include "netkit/nq_utils.h"
#include "netkit/buffer.h"
#include "connection_info.h"
#include "utils.h"
#include <functional>

namespace netkit {

class Connection final {
public:
    Connection(int fd, NotificationQueueImpl* new_rd_nq, void* client_ptr, Logger* l)
        : m_new_rd_nq(new_rd_nq), m_client_ptr(client_ptr), m_logger(l) {
        utils::GenConnectionInfo(fd, &m_info);
    }

    const ConnectionInfo& GetInfo() const {
        return m_info;
    }

    /** returns -errno or fd of the timer */
    int AddTimer(const TimeVal& delay, const TimeVal& interval,
                 /*
                   `val` < 0: error occurs and `val` == -errno
                   `val` > 0: the number of expirations
                   `out`: data needed to be sent in this connection
                 */
                 const std::function<void(int32_t val, netkit::Buffer* out)>&);

private:
    ConnectionInfo m_info;
    NotificationQueueImpl* m_new_rd_nq;
    void* m_client_ptr;
    Logger* m_logger;
};

}

#endif
