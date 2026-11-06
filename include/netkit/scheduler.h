#ifndef __NETKIT_SCHEDULER_H__
#define __NETKIT_SCHEDULER_H__

#include "event_queue.h"
#include <atomic>
#include <memory>
#include <vector>

namespace netkit {

class Scheduler final {
public:
    Scheduler(std::vector<std::unique_ptr<EventQueue>>* nl)
        : m_curr_idx(0), m_eq_list(nl) {}

    uint32_t GetWorkerNum() const {
        return m_eq_list->size();
    }

    int Schedule(int32_t res, void* tag, EventQueue* eq) {
        uint32_t idx = m_curr_idx.fetch_add(1, std::memory_order_acq_rel) %
            m_eq_list->size();
        return eq->NotifyAsync(m_eq_list->at(idx).get(), res, tag);
    }

    int Deliver(int32_t res, void* tag, EventQueue* eq, uint32_t idx) {
        if (idx >= m_eq_list->size()) {
            return -EINVAL;
        }
        return eq->NotifyAsync(m_eq_list->at(idx).get(), res, tag);
    }

private:
    std::atomic<uint32_t> m_curr_idx;
    std::vector<std::unique_ptr<EventQueue>>* m_eq_list;
};

}

#endif
