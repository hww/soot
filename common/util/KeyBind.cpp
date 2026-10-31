// common/util/KeyBind.cpp
#include "util/KeyBind.hpp"

#include "fmt/format.h"

namespace common {

    std::string KeyBind::string() const {
        switch (modifier) {
        case Modifier::CTRL: return fmt::format("CTRL-{}", key);
        case Modifier::SHIFT: return fmt::format("SHIFT-{}", key);
        case Modifier::META: return fmt::format("META-{}", key);
        }
        return "";
    }

} // namespace common