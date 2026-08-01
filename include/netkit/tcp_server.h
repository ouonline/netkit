#ifndef __NETKIT_TCP_SERVER_H__
#define __NETKIT_TCP_SERVER_H__

#include "tcp_client.h"
#include "event_handler.h"
#include "scheduler.h"
#include "logger/logger.h"

namespace netkit {

class TcpServer : public EventHandler {
public:
    void Init(int fd, Scheduler* sched, Logger* l) {
        m_fd = fd;
        m_sched = sched;
        m_logger = l;
    }

    int Start(NotificationQueue*);
    bool Process(EventResult, NotificationQueue*) final;

protected:
    virtual ~TcpServer();

    virtual TcpClient* CreateClient() = 0;

    // same API as other EventHandler derived classes
    Logger* logger() const {
        return m_logger;
    }

private:
    int m_fd = -1;
    Scheduler* m_sched = nullptr;
    Logger* m_logger = nullptr;
};

}

#endif
