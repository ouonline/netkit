#ifndef __NET_BUFFER_H__
#define __NET_BUFFER_H__

#include "status_code.h"
#include <stdint.h>
#include <vector>

namespace outils { namespace net {

class Buffer final {
public:
    char* Data() {
        return m_data.data();
    }
    uint32_t Size() const {
        return m_data.size();
    }
    StatusCode Resize(uint32_t new_size) {
        m_data.resize(new_size);
        return ((m_data.size() == new_size) ? SC_OK : SC_NOMEM);
    }
    StatusCode Append(const char* data, uint32_t len) {
        m_data.insert(m_data.end(), data, data + len);
        return SC_OK;
    }

private:
    std::vector<char> m_data;
};

}} // namespace outils::net

#endif
