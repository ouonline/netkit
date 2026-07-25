#include "netkit/utils.h"
#include "netkit/connection.h"
#include <unistd.h> // close()
using namespace std;

namespace netkit {

const EndpointInfo& Connection::GetEndpointInfo() {
    if (m_info.remote_port == 0) {
        lock_guard<mutex> _l(lock);
        if (m_info.remote_port == 0) {
            utils::GenEndpointInfo(fd, &m_info);
        }
    }
    return m_info;
}

Connection::~Connection() {
    close(fd);
}

}
