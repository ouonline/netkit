#ifndef __NET_BUFFER_H__
#define __NET_BUFFER_H__

#include "status_code.h"
#include "deps/utils/qbuf.h"
#include <stdint.h>

namespace utils { namespace net {

class Buffer final {

public:
    Buffer() {
        qbuf_init(&m_data);
    }
    ~Buffer() {
        qbuf_destroy(&m_data);
    }
    char* Data() {
        return (char*)qbuf_data(&m_data);
    }
    uint32_t Size() const {
        return qbuf_size(&m_data);
    }
    StatusCode Reserve(uint32_t new_size) {
        if (qbuf_reserve(&m_data, new_size) == 0) {
            return SC_OK;
        }

        return SC_NOMEM;
    }
    StatusCode Resize(uint32_t new_size) {
        if (qbuf_resize(&m_data, new_size) == 0) {
            return SC_OK;
        }

        return SC_NOMEM;
    }
    StatusCode Append(const char* data, uint32_t len) {
        if (qbuf_append(&m_data, data, len) == 0) {
            return SC_OK;
        }

        return SC_NOMEM;
    }

private:
    struct qbuf m_data;
};

}}

#endif
