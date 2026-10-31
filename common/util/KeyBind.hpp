// common/util/KeyBind.hpp
#pragma once

#include <string>

namespace common {

    /// @brief One keyboard shortcut bound to a REPL command.
    struct KeyBind {
        enum class Modifier { CTRL, SHIFT, META };

        Modifier    modifier;
        std::string key;
        std::string description;
        std::string command;

        /// @brief "CTRL-Q", "SHIFT-L", "META-T".
        std::string string() const;
    };

} // namespace common