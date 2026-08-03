#include "netkit/iouring/notification_queue_impl.h"
#include <string.h> // strerror()
#include <functional>
using namespace std;

namespace netkit { namespace iouring {

int NotificationQueueImpl::Init(const Options& options, Logger* l) {
    if (m_logger) {
        return 0;
    }

    int err = io_uring_queue_init(options.queue_size, &m_ring, 0);
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

int NotificationQueueImpl::Next(EventResult* res, void** tag,
                                const TimeVal* timeout) {
    struct io_uring_cqe* cqe = nullptr;

    if (timeout) {
        if (timeout->tv_sec == 0 && timeout->tv_usec == 0) {
            int ret = io_uring_peek_cqe(&m_ring, &cqe);
            if (ret < 0) {
                if (ret != -EAGAIN && ret != -EINTR) {
                    logger_error(m_logger, "peek cqe failed: [%s].",
                                 strerror(-ret));
                }
                return ret;
            }
        } else {
            struct __kernel_timespec kts = {
                .tv_sec = timeout->tv_sec,
                .tv_nsec = timeout->tv_usec * 1000,
            };
            int ret = io_uring_wait_cqe_timeout(&m_ring, &cqe, &kts);
            if (ret < 0) {
                if (ret != -EAGAIN && ret != -EINTR) {
                    logger_error(m_logger,
                                 "wait cqe with timeout failed: [%s].",
                                 strerror(-ret));
                }
                return ret;
            }
        }
    } else {
        int ret = io_uring_wait_cqe(&m_ring, &cqe);
        if (ret < 0) {
            if (ret != -EAGAIN && ret != -EINTR) {
                logger_error(m_logger, "wait cqe failed: [%s].",
                             strerror(-ret));
            }
            return ret;
        }
    }

    if (cqe->res < 0) {
        res->val = 0;
        res->err = -cqe->res;
    } else {
        res->val = cqe->res;
        res->err = 0;
    }
    *tag = io_uring_cqe_get_data(cqe);

    io_uring_cqe_seen(&m_ring, cqe);

    return 0;
}

static int GenericAsync(struct io_uring* ring, Logger* logger,
                        const function<void(struct io_uring_sqe*)>& func) {
    int ret;
    auto sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        do {
            ret = io_uring_submit(ring);
        } while (ret == -EAGAIN || ret == -EINTR);
        if (ret < 0) {
            logger_error(logger, "io_uring_submit failed: [%s].",
                         strerror(-ret));
            return ret;
        }

        sqe = io_uring_get_sqe(ring);
        if (!sqe) {
            return -EAGAIN;
        }
    }

    func(sqe);

    do {
        ret = io_uring_submit(ring);
    } while (ret == -EINTR);
    if (ret < 0) {
        logger_error(logger, "io_uring_submit failed: [%s].", strerror(-ret));
        // clear sqe's content that are set in func()
        io_uring_prep_nop(sqe);
        io_uring_sqe_set_data(sqe, nullptr);
        return ret;
    }

    return 0;
}

int NotificationQueueImpl::AcceptAsync(uintptr_t fd, void* tag,
                                       bool multishot) {
    if (multishot) {
        return GenericAsync(
            &m_ring, m_logger, [fd, tag](struct io_uring_sqe* sqe) -> void {
                io_uring_prep_multishot_accept(sqe, fd, nullptr, nullptr, 0);
                io_uring_sqe_set_data(sqe, tag);
            });
    }

    return GenericAsync(&m_ring, m_logger,
                        [fd, tag](struct io_uring_sqe* sqe) -> void {
                            io_uring_prep_accept(sqe, fd, nullptr, nullptr, 0);
                            io_uring_sqe_set_data(sqe, tag);
                        });
}

int NotificationQueueImpl::ReadAsync(uintptr_t fd, void* buf, uint64_t sz,
                                     void* tag) {
    return GenericAsync(&m_ring, m_logger,
                        [fd, buf, sz, tag](struct io_uring_sqe* sqe) -> void {
                            io_uring_prep_read(sqe, fd, buf, sz, -1);
                            io_uring_sqe_set_data(sqe, tag);
                        });
}

int NotificationQueueImpl::WriteAsync(uintptr_t fd, const void* buf,
                                      uint64_t sz, void* tag) {
    return GenericAsync(&m_ring, m_logger,
                        [fd, buf, sz, tag](struct io_uring_sqe* sqe) -> void {
                            io_uring_prep_write(sqe, fd, buf, sz, -1);
                            io_uring_sqe_set_data(sqe, tag);
                        });
}

int NotificationQueueImpl::CloseAsync(uintptr_t fd, void* tag) {
    return GenericAsync(&m_ring, m_logger,
                        [fd, tag](struct io_uring_sqe* sqe) -> void {
                            io_uring_prep_close(sqe, fd);
                            io_uring_sqe_set_data(sqe, tag);
                        });
}

int NotificationQueueImpl::NotifyAsync(NotificationQueue* nq, int res,
                                       void* tag) {
    auto impl = static_cast<NotificationQueueImpl*>(nq);
    return GenericAsync(&m_ring, m_logger,
                        [impl, res, tag](struct io_uring_sqe* sqe) -> void {
                            io_uring_prep_msg_ring(sqe, impl->m_ring.ring_fd,
                                                   res, (uint64_t)tag, 0);
                            // skips the successful notification for this ring
                            io_uring_sqe_set_flags(sqe, IOSQE_CQE_SKIP_SUCCESS);
                        });
}

}}
