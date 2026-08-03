#ifndef __NETKIT_TCP_CLIENT_H__
#define __NETKIT_TCP_CLIENT_H__

#include "task.h"
#include "scheduler.h"

namespace netkit {

enum ReqStat {
    /* invalid request */
    INVALID = -1,

    /*
      ok, and `req_bytes` is set to the total size of the request.
      note that `req_bytes` may be less than the size of buffer.
    */
    VALID = 0,

    /*
      more data required.

      `req_bytes` is set to the number of bytes left at the current stage,
      or is set to 0 if the number of bytes cannot be determined.
    */
    MORE_DATA = 1,
};

class TcpClient : public EventHandler {
protected:
    virtual ~TcpClient() = default;

    virtual int OnConnected(SendContext*) = 0;
    virtual void OnDisconnected() = 0;
    virtual ReqStat Check(const Buffer&, uint32_t* req_bytes) = 0;
    virtual TaskPtr CreateTask() = 0;

    Logger* logger() const {
        return m_conn->logger;
    }

private:
    friend class TcpServer;
    friend class EventManager;

    // logger MUST be set first, in case derived classes use logger in their
    // destructors.
    void Init(Logger* l) {
        m_conn = std::make_shared<Connection>(l);
    }

    void SetVar(int fd, Scheduler* sched) {
        m_conn->fd = fd;
        m_sched = sched;
    }

    int Start(NotificationQueue*);
    bool Process(EventResult, NotificationQueue*) final;
    void DeleteSelf() final;

private:
    bool HandleInvalidRequest();
    bool HandleMoreDataRequest(uint32_t req_bytes, NotificationQueue*);

    // -1: error
    // 0: ok and return
    // 1: ok, and continue next ->Check()
    int HandleValidRequest(uint32_t req_bytes, NotificationQueue*);

private:
    uint64_t m_bytes_needed = 0;
    Buffer m_buf;
    Scheduler* m_sched = nullptr;
    std::shared_ptr<Connection> m_conn;
};

using TcpClientPtr = EventHandlerPtr<TcpClient>;

}

#endif
