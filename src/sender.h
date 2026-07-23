#ifndef __NETKIT_SENDER_H__
#define __NETKIT_SENDER_H__

#include "netkit/event_handler.h"
#include "netkit/connection.h"
#include <memory>

namespace netkit {

class Sender final : public EventHandler {
public:
    Sender(const std::shared_ptr<Connection>& c) : m_conn(c) {}
    int Start(NotificationQueue*);
    bool Process(int64_t, NotificationQueue*) override;

protected:
    ~Sender() = default;

private:
    std::shared_ptr<Connection> m_conn;
};

}

#endif
