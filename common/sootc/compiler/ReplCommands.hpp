// sootc/compiler/ReplCommands.hpp
#pragma once

#include "script/Object.hpp"

#include <string>
#include <string_view>
#include <vector>


namespace sootc {

    class Compiler;

    enum class ReplStatus; // forward, defined in Compiler.hpp

    namespace repl {

        enum class CommandGroup {
            Session, // help, exit, clear, reload
            Options, // opt, opt-reset
            Run,     // sc-run, st-eval
            Inspect, // sc-env, st-env, sc-macro?, symbols, ts-info, ast, sizes
            Save,    // save-session, save-module, save-module-deps
            GC,      // gc, gc-list
        };

        std::string_view group_name(CommandGroup g);

        using Handler = ReplStatus (Compiler::*)(const script::Object &);

        /// @brief One parameter of a REPL command.
        struct CommandParam {
            std::string_view name;        // "path", "out", "expr"
            std::string_view type;        // "string", "path", "expr", "name", ...
            std::string_view description; // "path to the .sc file"
        };

        /// @brief Description of one REPL command.
        ///
        /// @details The table in ReplCommands.cpp is the single source
        ///          of truth for: which names are commands, how they
        ///          are grouped in (help), what (help "cmd") prints,
        ///          and which handler runs. Adding a command is one
        ///          table entry plus one handler.
        struct CommandInfo {
            /// @brief Primary name, e.g. "opt". Used in (help) and
            ///        as the dispatch key.
            std::string_view long_name;

            /// @brief Short alias (up to 3 chars), or "" for none.
            std::string_view short_name;

            /// @brief Group in (help): Session, Options, Run, ...
            CommandGroup group;

            /// @brief Usage string for (help "cmd").
            std::string_view usage;

            /// @brief One-line description.
            std::string_view summary;

            /// @brief Extra hint printed by (help "cmd").
            /// @details "" = none. For `opt` this is
            ///          "see (opt) for the full list", because the
            ///          full list is generated from Config::fields().
            std::vector<CommandParam> params; // empty = no documented params

            /// @brief Handler. May be nullptr while porting.
            Handler handler;
        };

        const std::vector<CommandInfo> &commands();

        // Lookup by long name, short name, or old name.
        // Returns nullptr if not found.
        const CommandInfo *find_command(std::string_view name);

        // All names that should be recognised (long + short + old).
        // Used by try_handle_repl_command.
        bool is_command_name(std::string_view name);

    } // namespace repl

} // namespace sootc