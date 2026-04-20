#pragma once

#include "common/CommonTypes.hpp"
#include <vector>
#include <expected>
#include <string>

namespace sootc {

// Глобальное состояние для сериализации
struct GlobalState {
    std::vector<std::string> m_strings;
    
    u32 add_string(const std::string& str) {
        u32 index = static_cast<u32>(m_strings.size());
        m_strings.push_back(str);
        return index;
    }
};

}