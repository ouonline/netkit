#include "internal_client.h"
#include "netkit/processor_factory.h"
#include <memory>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring> // strerror
using namespace std;

namespace netkit { namespace tcp {

InternalClient::InternalClient(int fd, const shared_ptr<Processor>& p, threadkit::ThreadPool* tp, Logger* logger)
    : m_fd(fd), m_conn(fd, logger), m_processor(p), m_logger(logger), m_tp(tp) {
    m_processor->OnConnected(&m_conn);
    m_task = make_shared<ProcessorTask>(m_processor, &m_conn);
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
    RetCode sc = ReadData(m_fd, m_task->GetBuffer(), m_logger);
    if (sc != RC_SUCCESS) {
        return sc;
    }

    while (true) {
        uint64_t packet_bytes = 0;
        int ret = m_processor->CheckPacket(m_task->GetBuffer(), &packet_bytes);
        if (ret == Processor::PACKET_INVALID) {
            logger_error(m_logger, "check packet failed.");
            return RC_REQ_PACKET_ERR;
        }
        if (ret == Processor::PACKET_MORE_DATA) {
            return RC_SUCCESS;
        }
        if (ret != Processor::PACKET_SUCCESS) {
            logger_error(m_logger, "unknown packet state: [%d]", ret);
            return RC_REQ_PACKET_ERR;
        }

        auto last_req = m_task;
        m_task = make_shared<ProcessorTask>(m_processor, &m_conn);

        auto buf = last_req->GetBuffer();
        if (buf->GetSize() == packet_bytes) {
            m_tp->AddTask(last_req);
            return RC_SUCCESS;
        }

        auto new_buf = m_task->GetBuffer();
        new_buf->Append(buf->GetData() + packet_bytes, buf->GetSize() - packet_bytes);
        buf->Resize(packet_bytes);
        m_tp->AddTask(last_req);
    }

    return RC_SUCCESS;
}

void InternalClient::ShutDown() {
    m_processor->OnDisconnected(&m_conn);
}

}} // namespace netkit::tcp
