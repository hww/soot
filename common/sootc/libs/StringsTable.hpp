#pragma once

#include "common/CommonTypes.hpp"
#include <vector>
#include <expected>
#include <string>
#include <numeric>
#include <Assert.hpp>

namespace sootc {

    // Глобальное состояние для сериализации в файл
    // Используется в FileNode
    struct StringsTable {
    private:
        std::vector<char> m_bytes;   // с '\0' между строками
        std::vector<u32>  m_offsets; // m_offsets[i] = смещение i-й строки
        std::unordered_map<std::string, u32> m_index; // для dedup

    public:
        u32 lookup_or_add(const std::string &s) {
            auto it = m_index.find(s);
            if (it != m_index.end()) return it->second;

            u32 offset = static_cast<u32>(m_bytes.size());
            m_bytes.insert(m_bytes.end(), s.begin(), s.end());
            m_bytes.push_back('\0');
            m_offsets.push_back(offset);
            m_index[s] = offset;
            return offset;
        }
        
        u32 get_relative_address_of(const std::string &s) const {
            auto it = m_index.find(s);
            if (it == m_index.end()) throw std::runtime_error("string not in table: " + s);
            return it->second;
        }

        const std::vector<char> &bytes() const { return m_bytes; }
        const std::vector<u32>  &offsets() const { return m_offsets; }
        u64                      size() const { return m_bytes.size(); }
        const char              *get_at(u32 offset) const { return m_bytes.data() + offset; }
    };

}