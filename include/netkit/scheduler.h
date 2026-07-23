#ifndef __NETKIT_SCHEDULER_H__
#define __NETKIT_SCHEDULER_H__

#include "notification_queue.h"
#include <atomic>
#include <memory>
#include <vector>

namespace netkit {

class Scheduler final {
public:
    Scheduler(std::vector<std::unique_ptr<NotificationQueue>>* nl)
        : m_curr_idx(0), m_nq_list(nl) {}

    int Process(int32_t res, void* tag, NotificationQueue* nq) {
        uint32_t idx = m_curr_idx.fetch_add(1, std::memory_order_acq_rel) %
            m_nq_list->size();
        return nq->NotifyAsync(m_nq_list->at(idx).get(), res, tag);
    }

private:
    std::atomic<uint32_t> m_curr_idx;
    std::vector<std::unique_ptr<NotificationQueue>>* m_nq_list;
};

}

#endif
