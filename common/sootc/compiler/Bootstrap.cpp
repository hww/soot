// sootc/compiler/Bootstrap.cpp
//
// Loading the compiler's own bootstrap libraries: lib.st (the SOOT
// interpreter bootstrap) and lib.sc (the SOOTC compiler bootstrap).
// These run once, in the constructor, before any user code is
// compiled.
//
// The user profile is loaded from here as well, but it is optional:
// "#f" means "no profile", which is the default for non-interactive
// runs.

#include "sootc/compiler/Compiler.hpp"

#include "script/Interpreter.hpp"

#include "util/FileUtil.hpp"
#include "util/Log.hpp"
#include "util/ProjectConfig.hpp"

#include "fmt/format.h"

#include <filesystem>
#include <string>
#include <vector>

namespace sootc {

    namespace {

        // Strip a UTF-8 BOM if present. lib.st and lib.sc are edited
        // on Windows, where some editors add a BOM; without this, the
        // first form would be read with a stray U+FEFF in front of it.
        std::string strip_bom(std::string content) {
            if (content.size() >= 3 && static_cast<uint8_t>(content[0]) == 0xEF &&
                static_cast<uint8_t>(content[1]) == 0xBB &&
                static_cast<uint8_t>(content[2]) == 0xBF) {
                content.erase(0, 3);
            }
            return content;
        }

    } // namespace

    void Compiler::load_soot_bootstrap() {
        namespace fs = std::filesystem;

        std::vector<fs::path> search_paths;
        for (const auto &s : m_config.compiler.search_paths) search_paths.emplace_back(s);

        const fs::path found = common::find_project_file("lib.st", fs::path{}, search_paths);
        if (found.empty()) {
            lg::warn("lib.st not found");
            return;
        }

        lg::info("Loading SOOT bootstrap library {}", found.string());

        try {
            const std::string content = strip_bom(file_util::read_text(found));
            m_soot.eval_string(content, found.string());
            lg::info("Loaded SOOT bootstrap library {}", found.string());
        } catch (const std::exception &e) {
            lg::error("Failed to evaluate {}: {}", found.string(), e.what());
        }
    }

    void Compiler::load_soc_bootstrap() {
        namespace fs = std::filesystem;

        std::vector<fs::path> search_paths;
        for (const auto &s : m_config.compiler.search_paths) search_paths.emplace_back(s);

        const fs::path found = common::find_project_file("lib.sc", fs::path{}, search_paths);
        if (found.empty()) {
            lg::warn("lib.sc not found");
            return;
        }

        lg::info("Loading SOOTC bootstrap library {}", found.string());

        try {
            auto result = compile_file(found);
            if (!result) {
                lg::error("Failed to compile {}: {}", found.string(), result.error());
                return;
            }
            if (*result) { m_runtime->load_module(std::move(**result)); }
            lg::info("Loaded SOOTC bootstrap library {}", found.string());
        } catch (const std::exception &e) {
            lg::error("Exception while loading {}: {}", found.string(), e.what());
        }
    }

    void Compiler::load_user_profile() {
        if (m_config.compiler.user_profile == "#f") return;

        REPL::StartupFile startup =
            REPL::load_user_startup_file(m_config.compiler.user_profile, SootPlatform::Default);

        for (const auto &cmd : startup.run_before_listen) {
            try {
                auto result = interpret(cmd);
                (void)result;
            } catch (const std::exception &e) {
                print_warning(fmt::format("Failed to execute startup command: {}", e.what()));
            }
        }

        lg::info("Loaded user profile: {}", m_config.compiler.user_profile);
    }

} // namespace sootc