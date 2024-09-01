#ifndef __NETKIT_BUFFER_H__
#define __NETKIT_BUFFER_H__

#include "cutils/qbuf.h"
#include <stdint.h>

namespace netkit {

class Buffer final {
public:
    Buffer() {
        qbuf_init(&m_data);
    }
    ~Buffer() {
        qbuf_destroy(&m_data);
    }

    Buffer(Buffer&& b) {
        qbuf_move_construct(&b.m_data, &m_data);
    }
    Buffer(QBuf&& b) {
        qbuf_move_construct(&b, &m_data);
    }

    void operator=(Buffer&& b) {
        qbuf_move(&b.m_data, &m_data);
    }
    void operator=(QBuf&& b) {
        qbuf_move(&b, &m_data);
    }

    char* GetData() {
        return (char*)qbuf_data(&m_data);
    }
    const char* GetData() const {
        return (const char*)qbuf_data(&m_data);
    }

    uint32_t GetSize() const {
        return qbuf_size(&m_data);
    }

    bool IsEmpty() const {
        return qbuf_empty(&m_data);
    }

    int Reserve(uint64_t new_size) {
        return qbuf_reserve(&m_data, new_size);
    }

    int Resize(uint64_t new_size) {
        return qbuf_resize(&m_data, new_size);
    }

    int Assign(const char* data, uint64_t len) {
        return qbuf_assign(&m_data, data, len);
    }

    int Append(const char* data, uint64_t len) {
        return qbuf_append(&m_data, data, len);
    }

private:
    QBuf m_data;

private:
    Buffer(const Buffer&) = delete;
    void operator=(const Buffer&) = delete;
};

}

#endif
