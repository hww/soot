#pragma once

#include <gtest/gtest.h>

#include "carbon/file/Globals.hpp"
#include "carbon/vm/VirtualMachine.hpp"
#include "sootc/compiler/Compiler.hpp"
#include "type_system/TypeSystem.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

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
    ///     compared to re-running the full prelude;
    ///   * NOT clearing Globals between tests, because Globals is
    ///     the target of the prelude's load_module and tearing it
    ///     down would invalidate the prelude's entries.
    ///
    /// If a test genuinely needs a pristine Compiler, it can call
    /// Compiler::reload_environment() or create its own instance.
    class SootcTest : public ::testing::Test {
    protected:
        /// @brief Runs once per test binary, before any test in this fixture.
        /// @details Quietens the loggers so the test report is readable,
        ///          and constructs the shared Compiler. The Compiler's
        ///          constructor runs the SOOT and SOOTC preludes exactly
        ///          once for the entire test binary.
        static void SetUpTestSuite() {
            // Suppress info-level chatter during tests. Warnings and
            // errors are still shown so real problems surface.
            lg::set_stdout_level(lg::level::warn);
            lg::set_file_level(lg::level::warn);

            // Construct the shared Compiler. This is where lib.sot and
            // lib.soc are loaded — once for the whole test binary.
            CompilationOptions options;
            options.mode = CompilerMode::COMPILE_ONLY;
            options.user_profile = "#f";
            options.search_paths = {"soot_src", "tests/sootc"};

            s_compiler = std::make_unique<Compiler>(options, std::nullopt, "#f", nullptr);
        }

        /// @brief Runs once per test binary, after all tests in this fixture.
        static void TearDownTestSuite() { s_compiler.reset(); }

        /// @brief Runs before every test in the fixture.
        /// @details Resets the per-test state that could leak between
        ///          tests, without tearing down the shared Compiler.
        ///          Specifically:
        ///            * the TypeSystem's user-registered types are
        ///              dropped, so that each test starts with only
        ///              builtin types;
        ///            * the Compiler's constant pool and session
        ///              forms are cleared.
        ///
        ///          Globals is intentionally left alone: it is the
        ///          target of the prelude's load_module and holds
        ///          live pointers into the prelude's buffer. Clearing
        ///          it here would break the prelude for every
        ///          subsequent test.
        void SetUp() override {
            // Reset user-registered types. Builtin types are re-added
            // by the Compiler instance and remain valid.
            TypeSystem::instance().clear();
            TypeSystem::instance().add_builtin_types();

            // The Compiler itself keeps per-session state (constants,
            // session forms). It is exposed via compiler() so tests
            // could reset it if needed; for now, we do not touch it,
            // because the shared instance is the point of this fixture.
        }

        void TearDown() override {
            // Nothing to do here: the shared Compiler lives on.
        }

        /// @brief Compile a source string as if it were typed at the REPL.
        /// @details Returns nullptr on error; the error message is stored
        ///          in `out_error`.
        std::unique_ptr<carbon::BinaryFile> compile(const std::string &source,
                                                    std::string       *out_error = nullptr) {
            auto forms = s_compiler->get_soot_interpreter().get_reader().read_from_string(
                source, false, "<test>");
            if (forms.is_null()) {
                if (out_error) *out_error = "failed to read forms";
                return nullptr;
            }

            auto result = s_compiler->compile_file(forms, "<test>");
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
        ///          looked up by SID in Globals, which is populated from
        ///          the BinaryFile produced by compile().
        RunResult run(const std::string &source, const std::string &function_name) {
            RunResult out;

            auto file = compile(source, &out.error);
            if (!file) return out;

            if (!carbon::Globals::inst().load_module(std::move(*file))) {
                out.error = "failed to load module into Globals";
                return out;
            }

            auto &globals = carbon::Globals::inst();
            void *fn_ptr = globals.find_symbol_ptr(carbon::StringId(function_name));
            if (!fn_ptr) {
                out.error = "function '" + function_name + "' not found in Globals";
                return out;
            }

            auto                  *lambda = reinterpret_cast<carbon::ScriptLambda *>(fn_ptr);
            carbon::VirtualMachine vm;
            out.value = vm.execute_function(lambda, carbon::RunMode::Run);
            out.ok = true;
            return out;
        }

        Compiler &compiler() { return *s_compiler; }

    private:
        /// @brief Shared Compiler instance, constructed once per test binary.
        /// @details Declared inline static so that every test in this
        ///          fixture sees the same instance without needing a
        ///          separate definition file.
        inline static std::unique_ptr<Compiler> s_compiler;
    };

} // namespace sootc::testing