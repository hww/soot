#pragma once

#include "common/CommonTypes.hpp"
#include "fmt/format.h"
#include <string>

namespace sootc {

class FunctionEnv;

struct Label {
    std::string name;
    FunctionEnv* func = nullptr;
    u64 offset = 0;
    
    Label() = default;
    Label(const std::string& n, FunctionEnv* f = nullptr) : name(n), func(f) {}
    
    bool is_resolved() const { return offset != 0; }
    
    std::string print() const {
        return fmt::format("{}@{}", name.empty() ? "L" + std::to_string(offset) : name, offset);
    }
};

} // namespace sootc