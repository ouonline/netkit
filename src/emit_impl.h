#ifndef __NETKIT_EMIT_IMPL_H__
#define __NETKIT_EMIT_IMPL_H__

#include "netkit/event_queue.h"
#include "netkit/connection.h"
#include <memory>
#include <functional>

namespace netkit {

void EmitImpl(Buffer&&, const std::shared_ptr<Connection>&,
              EventQueue* curr_eq, EventQueue* target_eq,
              const std::function<void(int err)>& on_complete, Logger*);
}

#endif
