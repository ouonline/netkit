#ifndef __NETKIT_INTERNAL_SERVER_H__
#define __NETKIT_INTERNAL_SERVER_H__

#include "netkit/request_handler_factory.h"
#include "state.h"
#include <memory>
#include <unistd.h>

namespace netkit {

struct InternalServer final : public State {
    InternalServer(int _fd, const std::shared_ptr<RequestHandlerFactory>& f)
        : fd(_fd), factory(f) {}

    ~InternalServer() {
        if (fd > 0) {
            close(fd);
        }
    }

    int fd;
    std::shared_ptr<RequestHandlerFactory> factory;
};

}

#endif