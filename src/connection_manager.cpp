#include "netkit/connection_manager.h"
#include "iouring_utils.h"
#include "utils.h"
#include <cstring> // strerror()
using namespace std;

namespace netkit {

RetCode ConnectionManager::Init() {
    int err = io_uring_queue_init(64, &m_ring, 0);
    if (err) {
        logger_error(m_logger, "io_uring_queue_init failed: [%s].", strerror(err));
        return RC_INTERNAL_NET_ERR;
    }
    return RC_OK;
}

ConnectionManager::~ConnectionManager() {
    io_uring_queue_exit(&m_ring);
}

RetCode ConnectionManager::CreateTcpServer(const char* addr, uint16_t port, void* tag) {
    int fd = utils::CreateTcpServerFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "create server failed.");
        return RC_INTERNAL_NET_ERR;
    }

    auto rc = utils::GenericAsync(&m_ring, m_logger, [fd, tag](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_multishot_accept(sqe, fd, nullptr, nullptr, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
    if (rc != RC_OK) {
        logger_error(m_logger, "register accept event failed.");
        shutdown(fd, SHUT_RDWR);
        return rc;
    }

    return RC_OK;
}

RetCode ConnectionManager::CreateTcpClient(const char* addr, uint16_t port, Connection* c) {
    int fd = utils::CreateTcpClientFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "create tcp client failed.");
        return RC_INTERNAL_NET_ERR;
    }

    c->Init(fd, &m_ring, m_logger);
    return RC_OK;
}

void ConnectionManager::InitializeConnection(int fd, Connection* conn) {
    conn->Init(fd, &m_ring, m_logger);
}

RetCode ConnectionManager::Loop(const function<void(uint64_t, void*)>& func) {
    while (true) {
        struct io_uring_cqe* cqe = nullptr;

        int ret = io_uring_wait_cqe(&m_ring, &cqe);
        if (ret < 0) {
            logger_error(m_logger, "wait cqe failed: [%s].", strerror(-ret));
            continue;
        }

        auto cqe_res = cqe->res;
        io_uring_cqe_seen(&m_ring, cqe);

        if (cqe_res < 0) {
            logger_error(m_logger, "async event failed: [%s].", strerror(-cqe_res));
            continue;
        }

        func(cqe_res, io_uring_cqe_get_data(cqe));
    }

    return RC_OK;
}

}
