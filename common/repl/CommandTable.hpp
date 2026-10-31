// common/repl/CommandTable.hpp
#pragma once

#include "util/Config.hpp"

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

    // Which modes a command is available in. Compile is the
    // superset: the compiler uses the interpreter internally, so
    // st-eval/st-env work there too. Interpret has only the
    // interpreter-side commands.
    enum class CommandMode : uint8_t {
        Interpret = 1,
        Compile = 2,
        Both = Interpret | Compile,
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

        /// @brief Where this command can be used, ...
        CommandMode modes = CommandMode::Both;

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


    bool is_available_for(common::CompilerConfig::Mode mode) const {
            CommandMode m = (mode == common::CompilerConfig::Mode::Compile)
                                ? CommandMode::Compile
                                : CommandMode::Interpret;
            return (static_cast<uint8_t>(m) & static_cast<uint8_t>(modes)) != 0;
        }

    };
   

    const CommandInfo *find_command(const std::vector<CommandInfo> &table, std::string_view name,
                                    common::CompilerConfig::Mode mode);

    bool is_command_name(const std::vector<CommandInfo> &table, std::string_view name,
                         common::CompilerConfig::Mode mode);

} // namespace repl