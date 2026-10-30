#pragma once

#include <gtest/gtest.h>

#include "carbon/Carbon.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/vm/VirtualMachine.hpp"
#include "sootc/compiler/Compiler.hpp"
#include "sootc/node/FileNode.hpp"
#include "sootc/node/NodeInspector.hpp"
#include "type_system/TypeSystem.hpp"
#include "util/Config.hpp"
#include "util/Log.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <ProjectConfig.hpp>

namespace sootc::testing {

    /// @brief Result of a compile + run cycle.
    struct RunResult {
        bool            ok = false;
        std::string     error;
        carbon::Variant value;
    };

    /// @brief Test fixture that brings up a Compiler in REPL-like mode.
    ///
    /// The Compiler, its TypeSystem, and the SOOT/SOOTC preludes are
    /// process-wide resources. Reloading lib.soc for every test is
    /// both slow (about 120 ms each time) and noisy (dozens of log
    /// lines per test). We therefore build a single Compiler in
    /// SetUpTestSuite and reuse it for the whole test binary.
    ///
    /// Per-test isolation is achieved by:
    ///   * clearing the constant pool and the session forms, which
    ///     live on the Compiler instance and are cheap to reset;
    ///   * re-registering only the builtin types, which is cheap
    ///     compared to re-running the full prelude.
    ///
    /// A shared Carbon context is created once per test binary and
    /// used for every load_module / execute_function call. It is NOT
    /// torn down between tests, because the prelude's modules live
    /// inside it and dropping it would invalidate the prelude.
    class SootcTest : public ::testing::Test {
    protected:
        static void SetUpTestSuite() {
            lg::set_console_level(lg::level::warn);
            lg::set_file_level(lg::level::warn);

            // The Compiler stores a const reference to Config, so
            // s_config must outlive s_compiler. Both are static, so
            // their lifetime is the test binary's lifetime.
            s_config = std::make_unique<common::Config>();
            // Find the project root the same way main() does: walk up from
            // CWD looking for soot.json. If not found, fall back to CWD.
            // Using CWD directly would make the tests depend on where the
            // test binary happens to live (build/Debug/bin), not on where
            // the project is.
            auto root = common::SootProjectConfig::find(std::filesystem::current_path());
            s_config->common.project_root = root.empty() ? std::filesystem::current_path() : root;
            s_config->compiler.mode = "compile";
            s_config->compiler.user_profile = "#f";
            s_config->compiler.search_paths = {"soot_src", "tests/sootc"};

            s_compiler = std::make_unique<Compiler>(*s_config, std::nullopt, "#f", nullptr);

            s_carbon = carbon::Carbon::current();
            if (!s_carbon) {
                throw std::runtime_error("SootcTest: Compiler did not create a Carbon context");
            }
        }

        static void TearDownTestSuite() {
            s_carbon = nullptr;
            s_compiler.reset();
            s_config.reset();
        }

        void SetUp() override {
            // Reset user-registered types. Builtin types are re-added
            // by the Compiler instance and remain valid.
            TypeSystem::instance().clear();
            TypeSystem::instance().add_builtin_types();

            // Carbon and Compiler deliberately survive between tests:
            // the prelude's modules live inside them.
        }

        void TearDown() override {}

        void dump_ast(const std::string &filename = "") {
            if (filename.empty()) {
                fmt::print("=== GlobalNode ({}) ===\n", compiler().global_env()->symbols().size());
                inspect_ast(compiler().global_env());
                return;
            }
            if (auto *fn = compiler().find_file_node(filename)) {
                fmt::print("--- {} ---\n", filename);
                inspect_ast(fn);
            } else {
                fmt::print("FileNode '{}' not found\n", filename);
            }
        }

        /// @brief Compile a source string as if it were typed at the REPL.
        /// @details Returns nullptr on error; the error message is stored
        ///          in `out_error`.
        std::unique_ptr<carbon::BinaryFile> compile(const std::string &source,
                                                    std::string       *out_error = nullptr,
                                                    bool               inspect_ast = false) {
            static std::size_t s_counter = 0;
            const std::string  filename = "<test-" + std::to_string(s_counter++) + ">";

            auto forms = s_compiler->get_soot_interpreter().get_reader().read_from_string(
                source, false, filename);
            if (forms.is_null()) {
                if (out_error) *out_error = "failed to read forms";
                return nullptr;
            }

            // dump_ast is read by compile_file at the moment it runs,
            // through m_config (which is a reference to s_config).
            s_config->compiler.dump_ast = inspect_ast;

            s_compiler->set_input_origin(InputOrigin::Test);
            auto result = s_compiler->compile_file(forms, filename);

            if (!result) {
                if (out_error) *out_error = result.error();
                return nullptr;
            }

            if (!*result) {
                if (out_error) *out_error = "no binary produced";
                return nullptr;
            }

            return std::move(*result);
        }

        /// @brief Compile the source, then run the named function in the VM.
        /// @details `source` must define the function. The function is
        ///          looked up by SID in the shared Carbon context.
        RunResult run(const std::string &source, const std::string &function_name,
                      bool inspect_ast = false) {
            RunResult out;

            auto file = compile(source, &out.error, inspect_ast);
            if (!file) return out;

            const std::string logical = "<test>/" + std::to_string(s_module_counter++);
            if (!s_carbon->load_module_from_binary(logical, std::move(*file))) {
                out.error = "failed to load module into Carbon";
                return out;
            }

            auto result = s_carbon->run_top_level(logical);
            if (!result.ok) {
                out.error = "top-level of module failed";
                return out;
            }

            void *fn_ptr = s_carbon->globals()->find_symbol_ptr(carbon::StringId(function_name));
            if (!fn_ptr) {
                out.error = "function '" + function_name + "' not found in Globals";
                return out;
            }

            auto *lambda = reinterpret_cast<carbon::ScriptLambda *>(fn_ptr);
            out.value = s_carbon->vm()->execute_function(lambda, carbon::RunMode::Run);
            out.ok = true;
            return out;
        }

        Compiler       &compiler() { return *s_compiler; }
        carbon::Carbon &carbon_context() { return *s_carbon; }

    private:
        inline static std::unique_ptr<common::Config> s_config;
        inline static std::unique_ptr<Compiler>       s_compiler;
        inline static carbon::Carbon                 *s_carbon = nullptr;
        inline static std::size_t                     s_module_counter = 0;
    };

} // namespace sootc::testing