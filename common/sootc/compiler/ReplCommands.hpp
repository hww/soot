// sootc/compiler/ReplCommands.hpp
#pragma once

#include "script/Object.hpp"

#include "repl/CommandTable.hpp"

#include <string>
#include <string_view>
#include <vector>


namespace sootc {

    class Compiler;

    enum class ReplStatus; // forward, defined in Compiler.hpp

    namespace repl_commands {

        const std::vector<::repl::CommandInfo> &commands();

    } // namespace repl

} // namespace sootc