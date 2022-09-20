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

InternalClient::InternalClient(int fd, const shared_ptr<ProcessorFactory>& factory, threadkit::ThreadPool* tp, Logger* logger)
    : m_fd(fd), m_bytes_needed(0), m_logger(logger), m_tp(tp), m_factory(factory), m_conn(fd, logger) {
    m_processor = CreateProcessor(factory, &m_conn);
    m_processor->OnConnected(&m_conn);
}

StatusCode InternalClient::ReadData() {
    int nbytes = 0;
    if (ioctl(m_fd, FIONREAD, &nbytes) != 0) {
        logger_error(m_logger, "ioctl failed: %s", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    auto buf = m_processor->GetPacket();
    const uint32_t end_offset = buf->GetSize();
    StatusCode sc = buf->Resize(buf->GetSize() + nbytes);
    if (sc != SC_OK) {
        logger_error(m_logger, "alloc mem failed: %u", sc);
        return sc;
    }

    char* cursor = buf->GetData() + end_offset;
    while (nbytes > 0) {
        int ret = read(m_fd, cursor, nbytes);
        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                buf->Resize(cursor - buf->GetData());
                break;
            }

            logger_error(m_logger, "read failed: %s", strerror(errno));
            return SC_INTERNAL_NET_ERR;
        }
        if (ret == 0) {
            return SC_CLIENT_DISCONNECTED;
        }

        nbytes -= ret;
        cursor += ret;
    }

    buf->Resize(cursor - buf->GetData());
    return SC_OK;
}

StatusCode InternalClient::In() {
    StatusCode sc = ReadData();
    if (sc != SC_OK) {
        return sc;
    }

    auto pkt = m_processor->GetPacket();
    if (m_bytes_needed > 0 && pkt->GetSize() < m_bytes_needed) {
        return SC_OK;
    }

    m_bytes_needed = 0;

    while (true) {
        uint32_t bytes_needed = 0;
        if (!m_processor->CheckPacket(&bytes_needed)) {
            logger_error(m_logger, "check packet failed: %d", bytes_needed);
            return SC_REQ_PACKET_ERR;
        }

        if (bytes_needed == 0) {
            return SC_OK;
        }

        auto buf = m_processor->GetPacket();
        if (buf->GetSize() < bytes_needed) {
            m_bytes_needed = bytes_needed;
            return SC_OK;
        }

        auto last_req = m_processor;
        m_processor = CreateProcessor(m_factory, &m_conn);

        if (buf->GetSize() == bytes_needed) {
            m_tp->AddTask(last_req);
            return SC_OK;
        }

        auto new_buf = m_processor->GetPacket();
        new_buf->Append(buf->GetData() + bytes_needed, buf->GetSize() - bytes_needed);
        buf->Resize(bytes_needed);
        m_tp->AddTask(last_req);
    }

    return SC_OK;
}

void InternalClient::Error() {
    m_processor->OnDisconnected(&m_conn);
}

}} // namespace netkit::tcp
