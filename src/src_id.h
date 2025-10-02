#pragma once

#include <source_location>
#include <string.h>
#include <stdio.h>

namespace nd {
class SrcId {
public:
    SrcId() : m_filename("??"), m_line(0) {}
    SrcId(const std::source_location& _loc) { Set(_loc); }
    ~SrcId() {}

    void Set(const std::source_location& _loc) {
        m_line = _loc.line();
        const char* filename = _loc.file_name();
        const char* ch = strrchr(filename, '/');
        if (ch) { 
            m_filename = ch + 1;
        } else if ((ch = strrchr(filename, '\\'))) {
            m_filename = ch + 1;
        } else if (filename) {
            m_filename = filename;
        } else {
            m_filename = "??";
        }
    }

    void Get(char* _buff, size_t _len) {
        snprintf(_buff, _len, "%s:%d", m_filename, m_line);
    }

private:
    const char* m_filename;
	uint_least32_t m_line; 
};
}
