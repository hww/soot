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
    /// Each test gets a fresh TypeSystem, a fresh Globals, and a fresh
    /// Compiler. The REPL is disabled because we are not testing it here.
    class SootcTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // The process-wide TypeSystem is a singleton, so clear it
            // before each test to avoid cross-test contamination.
            TypeSystem::instance().clear();

            // Globals is also a process-wide singleton. It holds pointers
            // into the previously-compiled module's buffer, which no longer
            // exists after the previous test tore down its Compiler.
            carbon::Globals::inst().clear_all();

            CompilationOptions options;
            options.mode = CompilerMode::COMPILE_ONLY;
            options.user_profile = "#f";
            options.search_paths = {"soot_src", "tests/sootc"};

            m_compiler = std::make_unique<Compiler>(options, std::nullopt, "#f", nullptr);
        }

        void TearDown() override { m_compiler.reset(); }

        /// @brief Compile a source string as if it were typed at the REPL.
        /// @details Returns nullptr on error; the error message is stored
        ///          in `out_error`.
        std::unique_ptr<carbon::BinaryFile> compile(const std::string &source,
                                                    std::string       *out_error = nullptr) {
            auto forms = m_compiler->get_soot_interpreter().get_reader().read_from_string(
                source, false, "<test>");
            if (forms.is_null()) {
                if (out_error) *out_error = "failed to read forms";
                return nullptr;
            }

            auto result = m_compiler->compile_file(forms, "<test>");
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

            carbon::Globals::inst().clear_all();
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

        Compiler &compiler() { return *m_compiler; }

    private:
        std::unique_ptr<Compiler> m_compiler;
    };

} // namespace sootc::testing