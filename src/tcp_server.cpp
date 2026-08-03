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

bool TcpServer::Process(EventResult res, NotificationQueue* nq) {
    if (res.err) {
        logger_error(m_logger, "server down: [%s].", strerror(res.err));
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
    client->Init(m_logger);
    client->SetVar(fd, m_sched);

    int err = client->Start(nq);
    if (err) {
        logger_error(m_logger, "TcpClient start failed: [%s].", strerror(-err));
        client->DeleteSelf();
        // fall through
    }

    return true;
}

}
