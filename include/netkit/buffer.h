#ifndef __NETKIT_BUFFER_H__
#define __NETKIT_BUFFER_H__

#include "retcode.h"
#include <stdint.h>
#include <vector>

namespace netkit {

class Buffer final {
public:
    char* GetData() {
        return m_data.data();
    }
    const char* GetData() const {
        return m_data.data();
    }
    uint32_t GetSize() const {
        return m_data.size();
    }
    RetCode Resize(uint32_t new_size) {
        m_data.resize(new_size);
        return ((m_data.size() == new_size) ? RC_SUCCESS : RC_NOMEM);
    }
    RetCode Append(const char* data, uint32_t len) {
        m_data.insert(m_data.end(), data, data + len);
        return RC_SUCCESS;
    }

private:
    std::vector<char> m_data;
};

} // namespace netkit

#endif
