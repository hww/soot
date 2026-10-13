#pragma once

#include <gtest/gtest.h>

#include "carbon/Carbon.hpp"
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
    ///     compared to re-running the full prelude.
    ///
    /// A shared Carbon context is created once per test binary and
    /// used for every load_module / execute_function call. It is NOT
    /// torn down between tests, because the prelude's modules live
    /// inside it and dropping it would invalidate the prelude.
    class SootcTest : public ::testing::Test {
    protected:
        static void SetUpTestSuite() {
            // Suppress info-level chatter during tests. Warnings and
            // errors are still shown so real problems surface.
            lg::set_stdout_level(lg::level::warn);
            lg::set_file_level(lg::level::warn);

            // Bring up the shared Carbon context first, so that any
            // code in the Compiler's constructor that reaches for
            // Carbon::current() finds a valid context.
            carbon::CarbonConfig carbon_cfg;
            carbon_cfg.register_builtin_types = true;
            s_carbon = std::make_unique<carbon::Carbon>();
            if (!s_carbon->initialize(carbon_cfg)) {
                throw std::runtime_error("SootcTest: failed to initialize Carbon");
            }

            // Construct the shared Compiler. This is where lib.sot and
            // lib.soc are loaded — once for the whole test binary.
            CompilationOptions options;
            options.mode = CompilerMode::COMPILE_ONLY;
            options.user_profile = "#f";
            options.search_paths = {"soot_src", "tests/sootc"};

            s_compiler = std::make_unique<Compiler>(options, std::nullopt, "#f", nullptr);
        }

        static void TearDownTestSuite() {
            s_compiler.reset();
            if (s_carbon) {
                s_carbon->shutdown();
                s_carbon.reset();
            }
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
        ///          looked up by SID in the shared Carbon context.
        RunResult run(const std::string &source, const std::string &function_name) {
            RunResult out;

            auto file = compile(source, &out.error);
            if (!file) return out;

            // Load the compiled module into the shared Carbon context
            // under a unique logical name so that repeated runs of the
            // same test do not collide. The name is derived from the
            // test name plus a monotonic counter.
            const std::string logical = "<test>/" + std::to_string(s_module_counter++);

            if (!s_carbon->load_module_from_binary(logical, std::move(*file))) {
                out.error = "failed to load module into Carbon";
                return out;
            }

            void *fn_ptr = s_carbon->globals()->find_symbol_ptr(carbon::StringId(function_name));
            if (!fn_ptr) {
                out.error = "function '" + function_name + "' not found in Globals";
                return out;
            }

            auto *lambda = reinterpret_cast<carbon::ScriptLambda *>(fn_ptr);
            auto *vm = s_carbon->vm();
            out.value = vm->execute_function(lambda, carbon::RunMode::Run);
            out.ok = true;
            return out;
        }

        Compiler       &compiler() { return *s_compiler; }
        carbon::Carbon &carbon_context() { return *s_carbon; }

    private:
        inline static std::unique_ptr<Compiler>       s_compiler;
        inline static std::unique_ptr<carbon::Carbon> s_carbon;
        inline static std::size_t                     s_module_counter = 0;
    };

} // namespace sootc::testing