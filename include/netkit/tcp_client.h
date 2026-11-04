#ifndef __NETKIT_TCP_CLIENT_H__
#define __NETKIT_TCP_CLIENT_H__

#include "task.h"
#include "scheduler.h"

namespace netkit {

enum PacketStat {
    /* invalid packet */
    INVALID = -1,

    /*
      ok, and `packet_bytes` is set to the total size of the packet.
      note that `packet_bytes` may be less than the size of buffer.
    */
    VALID = 0,

    /*
      more data required.

      `packet_bytes` is set to the number of bytes left at the current stage,
      or is set to 0 if the number of bytes cannot be determined.
    */
    MORE_DATA = 1,
};

class TcpClient : public EventHandler {
protected:
    TcpClient(Logger* l) : m_logger(l) {}
    virtual ~TcpClient() = default;

    // returns 0 or -errno
    virtual int OnConnected(SendContext*) = 0;
    virtual void OnDisconnected() = 0;
    virtual PacketStat Check(const Buffer&, uint32_t* packet_bytes) = 0;
    virtual TaskPtr CreateTask(Buffer&&) = 0;

protected:
    Logger* m_logger;

private:
    friend class TcpServer;
    friend class EventManager;

    void Init(int fd, Scheduler* sched) {
        m_conn = std::make_shared<Connection>(fd);
        m_sched = sched;
    }

    int Start(EventQueue*);
    bool DeliverTask(TaskPtr, EventQueue*);
    bool Process(EventResult, EventQueue*) final;
    void DeleteSelf() final;

private:
    int DoRead(void* buf, uint64_t sz, EventQueue*);
    void HandleInvalidPacket();
    int HandleMoreDataPacket(uint32_t packet_bytes, EventQueue*);

    // -1: error
    // 0: ok and return
    // 1: ok, and continue next ->Check()
    int HandleValidPacket(uint32_t packet_bytes, EventQueue*);

private:
    uint64_t m_bytes_needed = 0;
    Buffer m_buf;
    Scheduler* m_sched = nullptr;
    std::shared_ptr<Connection> m_conn;
};

using TcpClientPtr = EventHandlerPtr<TcpClient>;

}

#endif
