#include "netkit/iouring/notification_queue_impl.h"
#include <cstring> // strerror()
#include <functional>
using namespace std;

namespace netkit { namespace iouring {

int NotificationQueueImpl::Init(const NotificationQueueOptions& options, Logger* l) {
    if (m_logger) {
        return 0;
    }

    unsigned flags = 0;
    if (options.enable_kernel_polling) {
        flags |= IORING_SETUP_SQPOLL;
    }

    int err = io_uring_queue_init(256, &m_ring, flags);
    if (err) {
        logger_error(l, "io_uring_queue_init failed: [%s].", strerror(-err));
        return err;
    }

    m_logger = l;

    return 0;
}

void NotificationQueueImpl::Destroy() {
    if (m_logger) {
        io_uring_queue_exit(&m_ring);
        m_logger = nullptr;
    }
}

int NotificationQueueImpl::Next(int64_t* res, void** tag, struct timeval* timeout) {
    struct io_uring_cqe* cqe = nullptr;

    if (timeout) {
        if (timeout->tv_sec == 0 && timeout->tv_usec == 0) {
            int ret = io_uring_peek_cqe(&m_ring, &cqe);
            if (ret < 0) {
                return ret;
            }
        } else {
            struct __kernel_timespec kts = {
                .tv_sec = timeout->tv_sec,
                .tv_nsec = timeout->tv_usec * 1000,
            };
            int ret = io_uring_wait_cqe_timeout(&m_ring, &cqe, &kts);
            if (ret == -EAGAIN) {
                return ret;
            }
            if (ret < 0) {
                logger_error(m_logger, "wait cqe with timeout failed: [%s].", strerror(-ret));
                return ret;
            }
        }
    } else {
        int ret = io_uring_wait_cqe(&m_ring, &cqe);
        if (ret < 0) {
            logger_error(m_logger, "wait cqe failed: [%s].", strerror(-ret));
            return ret;
        }
    }

    *res = cqe->res;
    *tag = io_uring_cqe_get_data(cqe);

    io_uring_cqe_seen(&m_ring, cqe);

    return 0;
}

static int GenericAsync(struct io_uring* ring, Logger* logger,
                        const function<void(struct io_uring_sqe*)>& func) {
    int ret = 0;

    auto sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        ret = io_uring_submit(ring);
        if (ret < 0) {
            logger_error(logger, "io_uring_submit failed: [%s].", strerror(-ret));
            return ret;
        }

        sqe = io_uring_get_sqe(ring);
    }

    func(sqe);

    ret = io_uring_submit(ring);
    if (ret < 0) {
        logger_error(logger, "io_uring_submit failed: [%s].", strerror(-ret));
        return ret;
    }

    return 0;
}

int NotificationQueueImpl::MultiAcceptAsync(int64_t fd, void* tag) {
#ifdef NETKIT_IOURING_ENABLE_MULTI_ACCEPT
    return GenericAsync(&m_ring, m_logger, [fd, tag](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_multishot_accept(sqe, fd, nullptr, nullptr, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
#else
    (void)fd;
    (void)tag;
    logger_error(m_logger, "multi accept is not supported.");
    return -ENOSYS;
#endif
}

int NotificationQueueImpl::AcceptAsync(int64_t fd, void* tag) {
    return GenericAsync(&m_ring, m_logger, [fd, tag](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_accept(sqe, fd, nullptr, nullptr, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
}

int NotificationQueueImpl::ReadAsync(int64_t fd, void* buf, uint64_t sz, void* tag) {
    return GenericAsync(&m_ring, m_logger, [fd, buf, sz, tag](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_read(sqe, fd, buf, sz, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
}

int NotificationQueueImpl::WriteAsync(int64_t fd, const void* buf, uint64_t sz, void* tag) {
    return GenericAsync(&m_ring, m_logger, [fd, buf, sz, tag](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_write(sqe, fd, buf, sz, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
}

int NotificationQueueImpl::CloseAsync(int64_t fd, void* tag) {
    return GenericAsync(&m_ring, m_logger, [fd, tag](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_close(sqe, fd);
        io_uring_sqe_set_data(sqe, tag);
    });
}

}}
