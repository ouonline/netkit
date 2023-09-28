#include "netkit/iouring/notification_queue_impl.h"
#include <cstring> // strerror()
using namespace std;

namespace netkit { namespace iouring {

NotificationQueueImpl::NotificationQueueImpl() : m_logger(nullptr) {
    pthread_mutex_init(&m_producer_lock, nullptr);
}

NotificationQueueImpl::~NotificationQueueImpl() {
    Destroy();
    pthread_mutex_destroy(&m_producer_lock);
}

RetCode NotificationQueueImpl::Init(Logger* l) {
    if (m_logger) {
        return RC_OK;
    }

    int err = io_uring_queue_init(256, &m_ring, 0);
    if (err) {
        logger_error(l, "io_uring_queue_init failed: [%s].", strerror(err));
        return RC_INTERNAL_NET_ERR;
    }

    m_logger = l;

    return RC_OK;
}

void NotificationQueueImpl::Destroy() {
    if (m_logger) {
        io_uring_queue_exit(&m_ring);
        m_logger = nullptr;
    }
}

RetCode NotificationQueueImpl::MultiAcceptAsync(int64_t fd, void* tag) {
    return GenericAsync([fd, tag](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_multishot_accept(sqe, fd, nullptr, nullptr, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
}

RetCode NotificationQueueImpl::ReadAsync(int64_t fd, void* buf, uint64_t sz, void* tag) {
    return GenericAsync([fd, buf, sz, tag](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_read(sqe, fd, buf, sz, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
}

RetCode NotificationQueueImpl::WriteAsync(int64_t fd, const void* buf, uint64_t sz, void* tag) {
    return GenericAsync([fd, buf, sz, tag](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_write(sqe, fd, buf, sz, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
}

RetCode NotificationQueueImpl::Wait(int64_t* res, void** tag) {
    struct io_uring_cqe* cqe = nullptr;

    int ret = io_uring_wait_cqe(&m_ring, &cqe);
    if (ret < 0) {
        logger_error(m_logger, "wait cqe failed: [%s].", strerror(-ret));
        return RC_INTERNAL_NET_ERR;
    }

    *res = cqe->res;
    *tag = io_uring_cqe_get_data(cqe);

    io_uring_cqe_seen(&m_ring, cqe);

    return RC_OK;
}

RetCode NotificationQueueImpl::GenericAsync(const function<void(struct io_uring_sqe*)>& func) {
    int ret;
    pthread_mutex_lock(&m_producer_lock);

    auto sqe = io_uring_get_sqe(&m_ring);
    if (!sqe) {
        io_uring_submit(&m_ring);
        sqe = io_uring_get_sqe(&m_ring);
        if (!sqe) {
            logger_error(m_logger, "io_uring_get_sqe failed.");
            goto err;
        }
    }

    func(sqe);

    ret = io_uring_submit(&m_ring);
    if (ret <= 0) {
        logger_error(m_logger, "io_uring_submit failed: [%s].", strerror(-ret));
        goto err;
    }

    pthread_mutex_unlock(&m_producer_lock);
    return RC_OK;

err:
    pthread_mutex_unlock(&m_producer_lock);
    return RC_INTERNAL_NET_ERR;
}

}}
