#include "iouring_utils.h"
#include <cstring> // strerror()
using namespace std;

namespace netkit { namespace iouring {

RetCode GenericAsync(struct io_uring* ring, Logger* logger, const function<void(struct io_uring_sqe*)>& func) {
    auto sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        io_uring_submit(ring);
        sqe = io_uring_get_sqe(ring);
        if (!sqe) {
            logger_error(logger, "io_uring_get_sqe failed.");
            return RC_INTERNAL_NET_ERR;
        }
    }

    func(sqe);

    int ret = io_uring_submit(ring);
    if (ret <= 0) {
        logger_error(logger, "io_uring_submit failed: [%s].", strerror(-ret));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

}}
