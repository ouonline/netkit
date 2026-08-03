#ifndef __NETKIT_TCP_SERVER_H__
#define __NETKIT_TCP_SERVER_H__

#include "tcp_client.h"

namespace netkit {

class TcpServer : public EventHandler {
protected:
    virtual ~TcpServer();

    virtual TcpClientPtr CreateClient() = 0;

    // same API as other EventHandler derived classes
    Logger* logger() const {
        return m_logger;
    }

private:
    friend class EventManager;

    void Init(int fd, Scheduler* sched, Logger* l) {
        m_fd = fd;
        m_sched = sched;
        m_logger = l;
    }

    int Start(NotificationQueue*);
    bool Process(EventResult, NotificationQueue*) final;

private:
    int m_fd = -1;
    Scheduler* m_sched = nullptr;
    Logger* m_logger = nullptr;
};

using TcpServerPtr = EventHandlerPtr<TcpServer>;

}

#endif
