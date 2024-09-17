#ifndef __NETKIT_EVENT_MANAGER_H__
#define __NETKIT_EVENT_MANAGER_H__

#include "nq_utils.h"
#include "handler_factory.h"
#include <thread>
#include <memory>
#include <functional>

namespace netkit {

class EventManager final {
public:
    EventManager(Logger* logger) : m_logger(logger) {}

    /** returns 0 or -errno */
    int Init();

    /** returns -errno or index of the server */
    int AddServer(const char* addr, uint16_t port,
                  const std::shared_ptr<HandlerFactory>&);

    /** returns -errno or index of the client */
    int AddClient(const char* addr, uint16_t port,
                  const std::shared_ptr<Handler>&);

    /** returns -errno or index of the timer. timer will be deleted if error occurs */
    int AddTimer(const TimeVal& delay, const TimeVal& interval,
                 /*
                   `val` < 0: error occurs and `val` == -errno
                   `val` > 0: the number of expirations
                 */
                 const std::function<void(int32_t val)>& handler);

    void Loop();

private:
    int DoAddClient(int64_t new_fd, const std::shared_ptr<Handler>&);
    void ProcessWriting(int64_t, void* tag);
    void ProcessNewAndReading(int64_t, void* tag);
    /* |-- */ void HandleTimerExpired(int64_t res, void* state_ptr);
    /* |-- */ void HandleTimerNext(void* timer_ptr);
    /* |-- */ void HandleAccept(int64_t new_fd, void* svr_ptr);
    /* |-+ */ void HandleClientReading(int64_t, void* client_ptr);
    /*   |-- */ void HandleInvalidRequest(void* client_ptr);
    /*   |-- */ void HandleMoreDataRequest(void* client_ptr, uint64_t expand_size);
    /*   |__ */ bool HandleValidRequest(void* client_ptr, uint64_t req_bytes);

private:
    alignas(threadkit::CACHELINE_SIZE)

    Logger* m_logger;

    alignas(threadkit::CACHELINE_SIZE)

    NotificationQueueImpl m_wr_nq; // for writing events
    std::thread m_writing_thread;

    alignas(threadkit::CACHELINE_SIZE)

    std::unique_ptr<NotificationQueueImpl[]> m_worker_nq_list;
    std::vector<std::thread> m_worker_thread_list;

    uint32_t m_current_worker_idx = 0;
    uint32_t m_worker_num = 0;

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
