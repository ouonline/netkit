#ifndef __NETKIT_CONNECTION_MANAGER_H__
#define __NETKIT_CONNECTION_MANAGER_H__

#include "nq_utils.h"
#include "request_handler_factory.h"
#include "threadkit/threadpool.h"
#include <thread>
#include <memory>

namespace netkit {

class ConnectionManager final {
public:
    ConnectionManager(Logger* logger)
        : m_signal_nq_list(nullptr), m_logger(logger) {}

    int Init();
    int StartServer(const char* addr, uint16_t port,
                    const std::shared_ptr<RequestHandlerFactory>&);
    void Loop();

private:
    int DoAddClient(int64_t new_fd, const std::shared_ptr<RequestHandler>&);
    void ProcessNewAndReading(int64_t, void* tag);
    void HandleAccept(int64_t new_fd, void* svr);
    void HandleClientReading(int64_t, void* client);
    void HandleClientRequest(void* client);
    void HandleInvalidRequest(void* client);
    void HandleMoreDataRequest(void* client, uint64_t expand_size);
    void ProcessWriting(int64_t, void* tag);

private:
    // workers
    threadkit::ThreadPool m_thread_pool;

    // for writing events
    NotificationQueueImpl m_wr_nq;

    std::thread m_writing_thread;

    // for sending events to writing thread. one nq per thread.
    std::unique_ptr<NotificationQueueImpl[]> m_signal_nq_list;

    // for new connection and reading events
    NotificationQueueImpl m_new_rd_nq;

    Logger* m_logger;

private:
    ConnectionManager(ConnectionManager&&) = delete;
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(ConnectionManager&&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;
};

}

#endif