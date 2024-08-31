#include "netkit/sender.h"
#include "internal_client.h"
using namespace std;

namespace netkit {

const ConnectionInfo& Sender::GetConnectionInfo() const {
    return m_client->info;
}

int Sender::SendAsync(Buffer&& buf, const function<void(int)>& cb) {
    auto res = new Response();
    if (!res) {
        return -ENOMEM;
    }

    res->data = buf.Detach();
    res->client = m_client;
    res->callback = cb;
    GetClient(m_client);
    return m_signal_nq->NotifyAsync(m_wr_nq, 0, res);
}

}
