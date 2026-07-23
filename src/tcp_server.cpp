#include "netkit/tcp_server.h"
#include "misc.h"
#include "internal_utils.h"
#include <unistd.h> // close()
#include <string.h> // strerror()
using namespace std;

namespace netkit {

TcpServer::~TcpServer() {
    close(m_fd);
}

int TcpServer::Start(NotificationQueue* nq) {
    int err;
    do {
        err = nq->AcceptAsync(m_fd, static_cast<EventHandler*>(this), true);
    } while (ShouldRetry(err));
    if (err) {
        logger_error(m_logger, "add server to notification queue failed: [%s].",
                     strerror(-err));
        // fall through
    }
    return err;
}

bool TcpServer::Process(int64_t fd, NotificationQueue* nq) {
    if (fd < 0) {
        logger_error(m_logger, "server down: [%s].", strerror(-fd));
        return false;
    }

    auto client = CreateClient();
    if (!client) {
        close(fd);
        logger_error(m_logger, "create client failed.");
        return true;
    }

    client->Init(fd, m_sched, m_logger);

    int err = client->Start(nq);
    if (err) {
        logger_error(m_logger, "TcpClient start failed: [%s].", strerror(-err));
        client->DeleteSelf();
        // fall through
    }

    return true;
}

}
