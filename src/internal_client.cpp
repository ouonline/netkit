#include "internal_client.h"
#include "netkit/processor_factory.h"
#include <memory>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring> // strerror
using namespace std;

namespace netkit { namespace tcp {

static shared_ptr<Processor> CreateProcessor(const shared_ptr<ProcessorFactory>& factory, Connection* conn) {
    auto p = factory->CreateProcessor();
    p->SetConnection(conn);
    return shared_ptr<Processor>(p, [f = factory](Processor* t) -> void {
        f->DestroyProcessor(t);
    });
}

InternalClient::InternalClient(int fd, const shared_ptr<ProcessorFactory>& factory, threadkit::ThreadPool* tp,
                               Logger* logger)
    : m_fd(fd), m_packet_bytes(0), m_logger(logger), m_tp(tp), m_factory(factory), m_conn(fd, logger) {
    m_processor = CreateProcessor(factory, &m_conn);
    m_processor->OnConnected(&m_conn);
}

static RetCode ReadData(int fd, Buffer* buf, Logger* logger) {
    int nbytes = 0;
    if (ioctl(fd, FIONREAD, &nbytes) != 0) {
        logger_error(logger, "ioctl failed: %s", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    const uint32_t end_offset = buf->GetSize();
    RetCode sc = buf->Resize(buf->GetSize() + nbytes);
    if (sc != RC_SUCCESS) {
        logger_error(logger, "resize buffer failed: %u", sc);
        return sc;
    }

    char* cursor = buf->GetData() + end_offset;
    while (nbytes > 0) {
        int ret = read(fd, cursor, nbytes);
        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            logger_error(logger, "read failed: %s", strerror(errno));
            return RC_INTERNAL_NET_ERR;
        }
        if (ret == 0) {
            return RC_CLIENT_DISCONNECTED;
        }

        nbytes -= ret;
        cursor += ret;
    }

    buf->Resize(cursor - buf->GetData());
    return RC_SUCCESS;
}

RetCode InternalClient::In() {
    RetCode sc = ReadData(m_fd, m_processor->GetPacket(), m_logger);
    if (sc != RC_SUCCESS) {
        return sc;
    }

    auto pkt = m_processor->GetPacket();
    if (pkt->GetSize() < m_packet_bytes) {
        return RC_SUCCESS;
    }

    while (true) {
        if (!m_processor->CheckPacket(&m_packet_bytes)) {
            logger_error(m_logger, "check packet failed: %u", m_packet_bytes);
            return RC_REQ_PACKET_ERR;
        }

        auto buf = m_processor->GetPacket();
        if (buf->GetSize() < m_packet_bytes) {
            return RC_SUCCESS;
        }

        auto last_req = m_processor;
        m_processor = CreateProcessor(m_factory, &m_conn);

        if (buf->GetSize() == m_packet_bytes) {
            m_tp->AddTask(last_req);
            m_packet_bytes = 0; // clear m_packet_bytes because we don't know the size of next request
            return RC_SUCCESS;
        }

        auto new_buf = m_processor->GetPacket();
        new_buf->Append(buf->GetData() + m_packet_bytes, buf->GetSize() - m_packet_bytes);
        buf->Resize(m_packet_bytes);
        m_tp->AddTask(last_req);
    }

    return RC_SUCCESS;
}

void InternalClient::Error() {
    m_processor->OnDisconnected(&m_conn);
}

}} // namespace netkit::tcp
