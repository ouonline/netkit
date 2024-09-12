#ifndef __NETKIT_EVENT_MANAGER_H__
#define __NETKIT_EVENT_MANAGER_H__

#include "nq_utils.h"
#include "request_handler_factory.h"
#include "threadkit/threadpool.h"
#include <thread>
#include <memory>
#include <functional>

namespace netkit {

class EventManager final {
public:
    EventManager(Logger* logger) : m_logger(logger) {}

    int Init();
    int AddServer(const char* addr, uint16_t port,
                  const std::shared_ptr<RequestHandlerFactory>&);
    int AddClient(const char* addr, uint16_t port,
                  const std::shared_ptr<RequestHandler>&);
    /* will be deleted if `err` == -errno < 0 */
    int AddTimer(const TimeVal& delay, const TimeVal& interval,
                 const std::function<void(int err, uint64_t nr_expiration)>& handler);
    void Loop();

private:
    int DoAddClient(int64_t new_fd, const std::shared_ptr<RequestHandler>&);
    void ProcessNewAndReading(int64_t, void* tag);
    void HandleTimerEvent(int64_t, void* timer_handler);
    void HandleAccept(int64_t new_fd, void* svr);
    void HandleClientReading(int64_t, void* client);
    void HandleClientRequest(void* client);
    void HandleInvalidRequest(void* client);
    void HandleMoreDataRequest(void* client, uint64_t expand_size);
    void ProcessWriting(int64_t, void* tag);

private:
    alignas(threadkit::CACHELINE_SIZE)

    threadkit::ThreadPool m_thread_pool;
    Logger* m_logger;

    alignas(threadkit::CACHELINE_SIZE)

    // for writing events
    NotificationQueueImpl m_wr_nq;

    std::thread m_writing_thread;

    // for sending events to writing thread. one nq per thread.
    std::unique_ptr<NotificationQueueImpl[]> m_signal_nq_list;

    alignas(threadkit::CACHELINE_SIZE)

    // for new connection and reading events
    NotificationQueueImpl m_new_rd_nq;

private:
    EventManager(EventManager&&) = delete;
    EventManager(const EventManager&) = delete;
    void operator=(EventManager&&) = delete;
    void operator=(const EventManager&) = delete;
};

}

#endif
