// common/repl/CommandTable.hpp
#pragma once

#include <string_view>
#include <vector>

namespace sootc {
    class Compiler;
    enum class ReplStatus;
} // namespace sootc

namespace script {
    class Object;
} // namespace script

namespace repl {

    enum class CommandGroup {
        Session,
        Options,
        Run,
        Inspect,
        Save,
        GC,
    };

    std::string_view group_name(CommandGroup g);

    using Handler = sootc::ReplStatus (sootc::Compiler::*)(const script::Object &);

    /// @brief One parameter of a REPL command, for (help "cmd").
    struct CommandParam {
        std::string_view name;
        std::string_view type;
        std::string_view description;
    };

    /// @brief Description of one REPL command.
    ///
    /// @details The table is the single source of truth for: which
    ///          names are commands, how they are grouped in (help),
    ///          what (help "cmd") prints, and which handler runs.
    ///          Adding a command is one table entry plus one handler.
    struct CommandInfo {
        /// @brief Primary name, e.g. "opt". Used in (help) and as
        ///        the dispatch key.
        std::string_view long_name;

        /// @brief Short alias (up to 3 chars), or "" for none.
        std::string_view short_name;

        /// @brief Group in (help): Session, Options, Run, ...
        CommandGroup group;

        /// @brief Usage string for (help "cmd").
        std::string_view usage;

        /// @brief One-line description.
        std::string_view summary;

        /// @brief Parameters of the command, for (help "cmd").
        /// @details Empty if the command takes no parameters.
        ///          For `opt`, the option names and allowed values
        ///          are listed separately from Config::fields().
        std::vector<CommandParam> params;

        /// @brief Handler. May be nullptr while porting.
        Handler handler;
    };
    ;

    const CommandInfo *find_command(const std::vector<CommandInfo> &table, std::string_view name);

    bool is_command_name(const std::vector<CommandInfo> &table, std::string_view name);

} // namespace repl