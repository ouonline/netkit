#include "misc.h"
#include "netkit/tcp_client.h"
#include <string.h> // strerror()
using namespace std;

#define REQ_BUF_EXPAND_SIZE 1024

namespace netkit {

void TcpClient::DeleteSelf() {
    if (m_conn) {
        bool connected = (m_conn->fd >= 0);
        m_conn->ShutDown(m_logger);
        if (connected) {
            OnDisconnected();
        }
    }
    delete this;
}

int TcpClient::DoRead(void* buf, uint64_t sz, EventQueue* eq) {
loop:
    int err =
        eq->ReadAsync(m_conn->fd, buf, sz, static_cast<EventHandler*>(this));
    if (ShouldRetry(err)) {
        goto loop;
    }

    if (err) {
        logger_error(m_logger, "reading data failed: [%s].", strerror(-err));
        // fall through
    }

    return err;
}

int TcpClient::Start(EventQueue* eq) {
    int err = m_buf.Reserve(REQ_BUF_EXPAND_SIZE);
    if (err) {
        logger_error(m_logger, "reserve [%lu] bytes for packet failed: [%s].",
                     REQ_BUF_EXPAND_SIZE, strerror(-err));
        return err;
    }

    ClientContext ctx(m_conn, eq, m_logger);
    err = OnConnected(&ctx);
    if (err) {
        logger_error(m_logger, "client OnConnected failed: [%s].",
                     strerror(-err));
        return err;
    }

    return DoRead(m_buf.data(), REQ_BUF_EXPAND_SIZE, eq);
}

void TcpClient::HandleInvalidPacket() {
    const EndpointInfo& info = m_conn->GetEndpointInfo();
    logger_error(m_logger, "invalid packet from [%s:%u].",
                 info.remote_addr.c_str(), info.remote_port);
}

int TcpClient::HandleMoreDataPacket(uint32_t packet_bytes, EventQueue* eq) {
    if (packet_bytes == 0) {
        packet_bytes = REQ_BUF_EXPAND_SIZE;
    } else {
        m_bytes_needed = packet_bytes;
    }

    int err = m_buf.Reserve(m_buf.size() + packet_bytes);
    if (err) {
        logger_error(m_logger, "reserve [%lu] bytes failed: [%s].",
                     strerror(ENOMEM));
        return -ENOMEM;
    }

    return DoRead(m_buf.data() + m_buf.size(), packet_bytes, eq);
}

int TcpClient::HandleValidPacket(uint32_t packet_bytes, EventQueue* eq) {
    int err;

    Buffer pkt;
    if (packet_bytes < m_buf.size()) {
        err = pkt.Assign(m_buf.data() + packet_bytes,
                         m_buf.size() - packet_bytes);
        if (err) {
            logger_error(m_logger, "move packet data failed: [%s].",
                         strerror(ENOMEM));
            return -1;
        }
        m_buf.Resize(packet_bytes);
    }
    std::swap(pkt, m_buf);

    ScheduleContext sched_ctx(m_sched, eq, m_conn, m_logger);
    err = OnPacketReady(move(pkt), &sched_ctx);
    if (err) {
        logger_error(m_logger, "handle packet failed: [%s].",
                     strerror(-err));
        return -1;
    }

    if (m_buf.IsEmpty()) {
        err = HandleMoreDataPacket(0, eq);
        if (err) {
            return -1;
        }
        return 0;
    }

    return 1;
}

bool TcpClient::Process(EventResult res, EventQueue* eq) {
    if (res.val == 0) {
        return false;
    }

    uint32_t bytes_read = 0;
    if (res.val < 0) {
        if (!ShouldRetry(res.val)) {
            logger_error(m_logger, "read data failed: [%s].",
                         strerror(-res.val));
            m_conn->ShutDown(m_logger);
            return false;
        }
    } else {
        bytes_read = static_cast<uint32_t>(res.val);
        m_buf.Resize(m_buf.size() + bytes_read);
    }

    if (m_bytes_needed > 0) {
        m_bytes_needed -= bytes_read;
        if (m_bytes_needed > 0) {
            int err =
                eq->ReadAsync(m_conn->fd, m_buf.data() + m_buf.size(),
                              m_bytes_needed, static_cast<EventHandler*>(this));
            if (err) {
                logger_error(m_logger, "launch read packet failed: [%s].",
                             strerror(-err));
                return false;
            }
            return true;
        }
    }

    while (true) {
        PacketStatus pkt_stat = OnDataReady(m_buf);

        if (pkt_stat.status == PacketStatus::INVALID) {
            HandleInvalidPacket();
            return false;
        }

        if (pkt_stat.status == PacketStatus::MORE_DATA) {
            return (HandleMoreDataPacket(pkt_stat.bytes, eq) == 0);
        }

        int rc = HandleValidPacket(pkt_stat.bytes, eq);
        if (rc == -1) {
            return false;
        }
        if (rc == 0) {
            return true;
        }
    }

    return false; // unreachable
}

}
