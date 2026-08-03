#ifndef __NETKIT_TCP_SERVER_H__
#define __NETKIT_TCP_SERVER_H__

#include "tcp_client.h"

namespace netkit {

class TcpServer : public EventHandler {
protected:
    TcpServer(Logger* l) : m_logger(l) {}
    virtual ~TcpServer();

    virtual TcpClientPtr CreateClient() = 0;

protected:
    Logger* m_logger;

private:
    friend class EventManager;

    void Init(int fd, Scheduler* sched) {
        m_fd = fd;
        m_sched = sched;
    }

    int Start(NotificationQueue*);
    bool Process(EventResult, NotificationQueue*) final;

private:
    int m_fd = -1;
    Scheduler* m_sched = nullptr;
};

using TcpServerPtr = EventHandlerPtr<TcpServer>;

}

#endif
