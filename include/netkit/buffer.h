#ifndef __NETKIT_BUFFER_H__
#define __NETKIT_BUFFER_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace netkit {

class Buffer final {
public:
    Buffer() = default;
    ~Buffer() {
        free(m_data);
    }

    Buffer(Buffer&& b) {
        MoveConstruct(&b);
    }

    void operator=(Buffer&& b) {
        if (this != &b) {
            free(m_data);
            MoveConstruct(&b);
        }
    }

    char* data() {
        return (char*)m_data;
    }
    const char* data() const {
        return (const char*)m_data;
    }

    uint64_t size() const {
        return m_size;
    }

    bool IsEmpty() const {
        return (m_size == 0);
    }

    int Reserve(uint64_t new_size) {
        if (new_size <= m_capacity) {
            return 0;
        }

        uint64_t real_capacity = m_capacity == 0 ? 4 : m_capacity;
        do {
            real_capacity <<= 1;
        } while (real_capacity < new_size);

        void* new_data = realloc(m_data, real_capacity);
        if (!new_data) {
            return -1;
        }

        m_data = new_data;
        m_capacity = real_capacity;
        return 0;
    }

    int Resize(uint64_t new_size) {
        if (Reserve(new_size) != 0) {
            return -1;
        }

        m_size = new_size;
        return 0;
    }

    int Assign(const char* data, uint64_t len) {
        m_size = 0;
        return Append(data, len);
    }

    int Append(const char* data, uint64_t len) {
        const uint64_t new_size = m_size + len;
        if (Reserve(new_size) != 0) {
            return -1;
        }

        memcpy((char*)m_data + m_size, data, len);
        m_size = new_size;
        return 0;
    }

    int Append(const Buffer& b) {
        if (!b.IsEmpty()) {
            return Append(b.data(), b.size());
        }
        return 0;
    }

    void Clear() {
        m_size = 0;
    }

private:
    void MoveConstruct(Buffer* b) {
        m_data = b->m_data;
        m_size = b->m_size;
        m_capacity = b->m_capacity;

        b->m_data = nullptr;
        b->m_size = 0;
        b->m_capacity = 0;
    }

private:
    void* m_data = nullptr;
    uint64_t m_size = 0;
    uint64_t m_capacity = 0;

private:
    Buffer(const Buffer&) = delete;
    void operator=(const Buffer&) = delete;
};

}

#endif
