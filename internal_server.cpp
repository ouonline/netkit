#include "internal_server.h"
#include "internal_client.h"
#include "logger/global_logger.h"
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace utils { namespace net { namespace tcp {

StatusCode InternalServer::In() {
    int fd = accept(m_fd, nullptr, nullptr);
    if (fd == -1) {
        log_error("accept failed: %s.", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    auto client = new InternalClient(fd, m_factory, m_tp);
    if (!client) {
        log_error("allocate client failed.");
        goto err;
    }

    if (m_event_mgr->AddClient(client) == SC_OK) {
        return SC_OK;
    }

    log_error("add client failed: %s.", strerror(errno));
    delete client;
err:
    close(fd);
    return SC_INTERNAL_NET_ERR;
}

}}}
