#include "netkit/epoll/notification_queue_impl.h"
#include "netkit/epoll/event_handler.h"
#include <cstring> // strerror()
#include <unistd.h> // close()
#include <errno.h>

namespace netkit { namespace epoll {

RetCode NotificationQueueImpl::Init(Logger* l) {
    m_epfd = epoll_create(MAX_EVENTS);
    if (m_epfd < 0) {
        logger_error(l, "create epoll failed: %s.", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    m_logger = l;

    return RC_OK;
}

void NotificationQueueImpl::Destroy() {
    if (m_epfd > 0) {
        close(m_epfd);
        m_epfd = -1;
        m_event_idx = 0;
        m_nr_valid_event = 0;
    }
}

RetCode NotificationQueueImpl::Wait(int64_t* res, void** tag) {
    if (m_event_idx >= m_nr_valid_event) {
    again:
        m_nr_valid_event = epoll_wait(m_epfd, m_event_list, MAX_EVENTS, -1);
        if (m_nr_valid_event < 0) {
            if (errno == EINTR) {
                goto again;
            }
            logger_error(m_logger, "epoll_wait failed: %s", strerror(errno));
            return RC_INTERNAL_NET_ERR;
        }

        m_event_idx = 0;
    }

    uint32_t events = m_event_list[m_event_idx].events;
    auto handler = static_cast<EventHandler*>(m_event_list[m_event_idx].data.ptr);

    *tag = handler->tag;

    if ((events & EPOLLHUP) || (events & EPOLLRDHUP) || (events & EPOLLERR)) {
        *res = 0;
    } else if (events & EPOLLIN) {
        *res = handler->In();
    }

    ++m_event_idx;

    return RC_OK;
}

}}
