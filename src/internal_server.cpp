#include "internal_server.h"
#include "internal_client.h"
#include "utils.h"
#include <sys/socket.h>
#include <unistd.h>
using namespace std;

namespace netkit { namespace tcp {

RetCode InternalServer::In() {
    int fd = accept(m_fd, nullptr, nullptr);
    if (fd == -1) {
        logger_error(m_logger, "accept failed: %s.", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }
    if (SetNonBlocking(fd, m_logger) != RC_SUCCESS) {
        close(fd);
        return RC_INTERNAL_NET_ERR;
    }

    auto processor = shared_ptr<Processor>(m_factory->CreateProcessor(), [f = m_factory](Processor* t) -> void {
        f->DestroyProcessor(t);
    });

    auto client = new InternalClient(fd, processor, m_tp, m_logger);
    if (!client) {
        logger_error(m_logger, "allocate client failed.");
        goto err;
    }

    if (m_event_mgr->AddHandler(client, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLET) == RC_SUCCESS) {
        return RC_SUCCESS;
    }

    logger_error(m_logger, "add client failed: %s.", strerror(errno));
    delete client;
err:
    close(fd);
    return RC_INTERNAL_NET_ERR;
}

}} // namespace netkit::tcp
