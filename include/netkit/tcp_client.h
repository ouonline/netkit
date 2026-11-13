#ifndef __NETKIT_TCP_CLIENT_H__
#define __NETKIT_TCP_CLIENT_H__

#include "client_context.h"
#include "schedule_context.h"

namespace netkit {

struct PacketStatus final {
    enum {
        /* invalid packet */
        INVALID = -1,

        /*
          ok, and `bytes` is set to the total size of the packet.
          note that `bytes` may be less than size of the buffer.
        */
        VALID = 0,

        /*
          more data required.

          `bytes` is set to the number of bytes needed at current stage,
          or 0 if the number of bytes cannot be determined.
        */
        MORE_DATA = 1,
    } status;

    uint32_t bytes;
};

class TcpClient : public EventHandler {
protected:
    TcpClient(Logger* l) : m_logger(l) {}
    ~TcpClient() = default;

    // returns 0 or -errno
    virtual int OnConnected(ClientContext*) = 0;
    virtual void OnDisconnected() = 0;
    virtual PacketStatus OnDataReady(const Buffer&) = 0;
    virtual int OnPacketReady(Buffer&&, ScheduleContext*) = 0;

    void Emit(EventQueue* curr_eq, Buffer&& data,
              const std::function<void(int err)>& on_complete = {});

protected:
    Logger* m_logger;

private:
    friend class TcpServer;
    friend class EventManager;

    int Start(int fd, Scheduler*, EventQueue*);
    bool Process(EventResult, EventQueue*) final;
    void DeleteSelf() final;

private:
    int DoRead(void* buf, uint64_t sz, EventQueue*);
    void HandleInvalidPacket();
    int HandleMoreDataPacket(uint32_t packet_bytes, EventQueue*);

    // -1: error
    // 0: ok and return
    // 1: ok, and continue next ->OnDataReady()
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
