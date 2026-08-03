#include "misc.h"
#include "netkit/utils.h"
#include "netkit/event_manager.h"
#include "netkit/iouring/notification_queue_impl.h"
#include <string.h>
using namespace std;

namespace netkit {

using namespace iouring;

static void WorkLoop(NotificationQueue* nq, Logger* logger) {
    while (true) {
        EventResult res;
        void* tag = nullptr;
        int err = nq->Next(&res, &tag, nullptr);
        if (err) {
            logger_error(logger, "get event failed: [%s].", strerror(-err));
            break;
        }
        if (!tag) {
            break;
        }

        auto handler = static_cast<EventHandler*>(tag);
        bool keep = handler->Process(res, nq);
        if (!keep) {
            handler->DeleteSelf();
        }
    }
}

void EventManager::Destroy() {
    if (m_worker_thread_list.empty()) {
        return;
    }

    for (uint32_t i = 0; i < m_worker_nq_list.size(); ++i) {
        m_nq->NotifyAsync(m_worker_nq_list[i].get(), 0, nullptr);
    }

    for (uint32_t i = 0; i < m_worker_thread_list.size(); ++i) {
        if (m_worker_thread_list[i].joinable()) {
            m_worker_thread_list[i].join();
        }
    }

    m_worker_thread_list.clear();
    m_worker_nq_list.clear();
    m_nq.reset();
    m_logger = nullptr;
}

int EventManager::Init(const Options& options) {
    if (!m_worker_thread_list.empty()) {
        return 0;
    }

    uint32_t worker_num = 0;
    if (options.worker_num > 0) {
        worker_num = options.worker_num;
    } else {
        worker_num = max(thread::hardware_concurrency(), 2u) - 1;
    }

    m_worker_nq_list.resize(worker_num);
    for (uint32_t i = 0; i < worker_num; ++i) {
        auto impl = new NotificationQueueImpl();
        if (!impl) {
            logger_error(m_logger, "allocate notification queue failed: [%s].",
                         strerror(ENOMEM));
            return -ENOMEM;
        }
        m_worker_nq_list[i].reset(impl);

        int err = impl->Init(NotificationQueueImpl::Options(), m_logger);
        if (err) {
            logger_error(m_logger, "init notification queue failed: [%s].",
                         strerror(-err));
            return err;
        }
    }

    auto impl = new NotificationQueueImpl();
    if (!impl) {
        logger_error(m_logger, "allocate notification queue failed: [%s].",
                     strerror(ENOMEM));
        return -ENOMEM;
    }
    m_nq.reset(impl);

    int err = impl->Init(NotificationQueueImpl::Options(), m_logger);
    if (err) {
        logger_error(m_logger, "init notification queue failed: [%s].",
                     strerror(-err));
        return err;
    }

    m_worker_thread_list.reserve(worker_num);
    for (uint32_t i = 0; i < worker_num; ++i) {
        m_worker_thread_list.emplace_back(WorkLoop, m_worker_nq_list[i].get(),
                                          m_logger);
    }

    signal(SIGPIPE, SIG_IGN);
    return 0;
}

int EventManager::AddTcpServer(const char* addr, uint16_t port,
                               TcpServerPtr ptr) {
    if (!ptr) {
        return -EINVAL;
    }

    int fd = utils::CreateTcpServerFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "create server for [%s:%u] failed: [%s].", addr,
                     port, strerror(-fd));
        return fd;
    }

    TcpServer* svr = ptr.release();
    svr->Init(fd, &m_sched);

    int err = svr->Start(m_nq.get());
    if (err) {
        logger_error(m_logger, "start server failed: [%s].", strerror(-err));
        svr->DeleteSelf();
        return err;
    }

    return fd;
}

int EventManager::AddTcpClient(const char* addr, uint16_t port,
                               TcpClientPtr ptr) {
    if (!ptr) {
        return -EINVAL;
    }

    int fd = utils::CreateTcpClientFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "connect to [%s:%u] failed: [%s].", addr, port,
                     strerror(-fd));
        return fd;
    }

    TcpClient* client = ptr.release();
    client->Init(fd, &m_sched);

    int err = client->Start(m_nq.get());
    if (err) {
        logger_error(m_logger, "TcpClient start failed: [%s].", strerror(-err));
        client->DeleteSelf();
        return err;
    }

    return fd;
}

void EventManager::Loop() {
    WorkLoop(m_nq.get(), m_logger);
}

}
