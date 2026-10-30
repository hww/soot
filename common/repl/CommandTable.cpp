// common/repl/CommandTable.cpp

#include "repl/CommandTable.hpp"

namespace repl {

    std::string_view group_name(CommandGroup g) {
        switch (g) {
        case CommandGroup::Session: return "Session";
        case CommandGroup::Options: return "Options";
        case CommandGroup::Run: return "Run";
        case CommandGroup::Inspect: return "Inspect";
        case CommandGroup::Save: return "Save";
        case CommandGroup::GC: return "GC";
        }
        return "Other";
    }

    const CommandInfo *find_command(const std::vector<CommandInfo> &table, std::string_view name) {
        for (const auto &c : table) {
            if (c.long_name == name) return &c;
            if (!c.short_name.empty() && c.short_name == name) return &c;
        }
        return nullptr;
    }

    bool is_command_name(const std::vector<CommandInfo> &table, std::string_view name) {
        return find_command(table, name) != nullptr;
    }

} // namespace repl