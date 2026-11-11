#ifndef __NETKIT_CLIENT_CONTEXT_H__
#define __NETKIT_CLIENT_CONTEXT_H__

#include "timer.h"
#include <functional>

namespace netkit {

class ClientContext final {
public:
    ClientContext(const std::shared_ptr<Connection>& c, EventQueue* eq,
                  Logger* l)
        : m_conn(c), m_eq(eq), m_logger(l) {}

    const EndpointInfo& GetEndpointInfo() {
        return m_conn->GetEndpointInfo();
    }

    void Emit(Buffer&& data,
              const std::function<void(int err)>& on_complete = {}) {
        EmitImpl(std::move(data), nullptr, on_complete);
    }

    // returns -errno or timer fd
    int AddTimer(const TimeVal& interval, TimerPtr);

private:
    friend class TcpClient;

    void EmitImpl(Buffer&&, EventQueue* target_eq = nullptr,
                  const std::function<void(int err)>& on_complete = {});

private:
    const std::shared_ptr<Connection>& m_conn;
    EventQueue* m_eq;
    Logger* m_logger;
};

}

#endif
