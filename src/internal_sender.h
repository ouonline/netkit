#ifndef __NETKIT_INTERNAL_SENDER_H__
#define __NETKIT_INTERNAL_SENDER_H__

#include "netkit/event_handler.h"
#include "netkit/connection.h"
#include <memory>

namespace netkit {

class InternalSender final : public EventHandler {
protected:
    ~InternalSender() = default;

private:
    friend class ClientContext;

    InternalSender(const std::shared_ptr<Connection>& c, Logger* l)
        : m_conn(c), m_logger(l) {}
    int Start(EventQueue*);
    int DoWrite(const void* buf, uint64_t sz, EventQueue*);
    bool Process(EventResult, EventQueue*) override;

private:
    std::shared_ptr<Connection> m_conn;
    Logger* m_logger;
};

}

#endif
