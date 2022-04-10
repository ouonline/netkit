#include "internal_client.h"
#include "netkit/processor_factory.h"
#include <memory>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring> // strerror
using namespace std;

namespace outils { namespace net { namespace tcp {

InternalClient::InternalClient(int fd, const shared_ptr<ProcessorFactory>& factory, ThreadPool* tp, Logger* logger)
    : m_fd(fd), m_bytes_needed(0), m_logger(logger), m_tp(tp), m_factory(factory), m_conn(fd, logger) {
    m_processor = CreateProcessor();
    factory->OnClientConnected(&m_conn);
}

StatusCode InternalClient::ReadData() {
    int nbytes = 0;
    if (ioctl(m_fd, FIONREAD, &nbytes) != 0) {
        logger_error(m_logger, "ioctl failed: %s", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    auto buf = m_processor->GetPacket();
    const uint32_t end_offset = buf->Size();
    StatusCode sc = buf->Resize(buf->Size() + nbytes);
    if (sc != SC_OK) {
        logger_error(m_logger, "alloc mem failed: %u", sc);
        return sc;
    }

    char* cursor = buf->Data() + end_offset;
    while (nbytes > 0) {
        int ret = read(m_fd, cursor, nbytes);
        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                buf->Resize(cursor - buf->Data());
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

    buf->Resize(cursor - buf->Data());
    return SC_OK;
}

Processor* InternalClient::CreateProcessor() {
    auto p = m_factory->CreateProcessor();
    p->SetConnection(&m_conn);
    return p;
}

StatusCode InternalClient::In() {
    StatusCode sc = ReadData();
    if (sc != SC_OK) {
        return sc;
    }

    auto pkt = m_processor->GetPacket();
    if (m_bytes_needed > 0 && pkt->Size() < m_bytes_needed) {
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
        if (buf->Size() < bytes_needed) {
            m_bytes_needed = bytes_needed;
            return SC_OK;
        }

        auto last_req = m_processor;
        m_processor = CreateProcessor();

        if (buf->Size() == bytes_needed) {
            m_tp->AddTask(shared_ptr<Processor>(last_req, [this](Processor* t) -> void {
                m_factory->DestroyProcessor(t);
            }));
            return SC_OK;
        }

        auto new_buf = m_processor->GetPacket();
        new_buf->Append(buf->Data() + bytes_needed, buf->Size() - bytes_needed);
        buf->Resize(bytes_needed);
        m_tp->AddTask(shared_ptr<Processor>(last_req, [this](Processor* t) -> void {
            m_factory->DestroyProcessor(t);
        }));
    }

    return SC_OK;
}

void InternalClient::Error() {
    m_factory->OnClientDisconnected(&m_conn);
}

}}} // namespace outils::net::tcp
