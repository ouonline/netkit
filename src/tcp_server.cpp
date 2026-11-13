#include "netkit/tcp_server.h"
#include "misc.h"
#include <unistd.h> // close()
#include <string.h> // strerror()
using namespace std;

namespace netkit {

TcpServer::~TcpServer() {
    if (m_fd >= 0) {
        close(m_fd);
    }
}

int TcpServer::Start(EventQueue* eq) {
    int err;

    while (true) {
        err = eq->AcceptAsync(m_fd, static_cast<EventHandler*>(this), true);
        if (!ShouldRetry(err)) {
            break;
        }
    }

    if (err) {
        logger_error(m_logger, "add server to event queue failed: [%s].",
                     strerror(-err));
        // fall through
    }

    return err;
}

bool TcpServer::Process(EventResult res, EventQueue* eq) {
    if (res.val < 0) {
        logger_error(m_logger, "server down: [%s].", strerror(-res.val));
        return false;
    }

    int fd = res.val;
    TcpClientPtr ptr = CreateClient();
    if (!ptr) {
        close(fd);
        logger_error(m_logger, "create client failed.");
        return true;
    }

    TcpClient* client = ptr.release();
    int err = client->Start(fd, m_sched, eq);
    if (err) {
        logger_error(m_logger, "TcpClient start failed: [%s].", strerror(-err));
        client->DeleteSelf();
        // fall through
    }

    return true;
}

}
