#ifndef __NETKIT_SENDER_H__
#define __NETKIT_SENDER_H__

#include "netkit/event_handler.h"
#include "netkit/connection.h"
#include <memory>

namespace netkit {

class Sender final : public EventHandler {
protected:
    ~Sender() = default;

private:
    friend class SendContext;

    Sender(const std::shared_ptr<Connection>& c) : m_conn(c) {}
    int Start(NotificationQueue*);
    bool Process(EventResult, NotificationQueue*) override;

    int DoWrite(const void* buf, uint64_t sz, NotificationQueue*);

private:
    std::shared_ptr<Connection> m_conn;
};

}

#endif
