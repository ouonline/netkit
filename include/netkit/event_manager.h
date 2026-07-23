#ifndef __NETKIT_EVENT_MANAGER_H__
#define __NETKIT_EVENT_MANAGER_H__

#include "tcp_server.h"
#include "logger/logger.h"
#include <memory>
#include <thread>

namespace netkit {

class EventManager final {
public:
    struct Options final {
        uint32_t worker_num = 0;
    };

public:
    EventManager(Logger* logger)
        : m_logger(logger), m_sched(&m_worker_nq_list) {}
    ~EventManager() {
        Destroy();
    }

    /** returns 0 or -errno */
    int Init(const Options&);
    void Destroy();

    /** returns -errno or fd of the server */
    int AddTcpServer(const char* addr, uint16_t port, TcpServer*);

    /** returns -errno or fd of the client */
    int AddTcpClient(const char* addr, uint16_t port, TcpClient*);

    void Loop();

private:
    Logger* m_logger;
    std::unique_ptr<NotificationQueue> m_new_rd_nq;
    std::vector<std::unique_ptr<NotificationQueue>> m_worker_nq_list;
    Scheduler m_sched;
    std::vector<std::thread> m_worker_thread_list;

private:
    EventManager(EventManager&&) = delete;
    EventManager(const EventManager&) = delete;
    void operator=(EventManager&&) = delete;
    void operator=(const EventManager&) = delete;
};

}

#endif
