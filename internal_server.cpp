#include "internal_server.h"
#include "internal_client.h"
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace utils { namespace net { namespace tcp {

StatusCode InternalServer::In() {
    int fd = accept(m_fd, nullptr, nullptr);
    if (fd == -1) {
        logger_error(m_logger, "accept failed: %s.", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    auto client = new InternalClient(fd, m_factory, m_tp, m_logger);
    if (!client) {
        logger_error(m_logger, "allocate client failed.");
        goto err;
    }

    if (m_event_mgr->AddClient(client) == SC_OK) {
        return SC_OK;
    }

    logger_error(m_logger, "add client failed: %s.", strerror(errno));
    delete client;
err:
    close(fd);
    return SC_INTERNAL_NET_ERR;
}

}}}
