// sootc/compiler/Compiler.hpp
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "fmt/color.h"
#include "fmt/format.h"

#include "sootc/compiler/CompilerError.hpp"

#include "sootc/node/GlobalNode.hpp"
#include "sootc/node/Node.hpp"
#include "sootc/node/NoneNode.hpp"

#include "common/soot/Interpreter.hpp"
#include "common/soot/Object.hpp"
#include "common/soot/Reader.hpp"

#include "carbon/file/BinaryFile.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/vm/VirtualMachine.hpp"
#include "type_system/Deftype.hpp"
#include "type_system/TypeSystem.hpp"

namespace sootc {

    // Compiler operation mode.
    enum class CompilerMode {
        COMPILE_ONLY,   // Only compile; do not interpret at REPL.
        INTERPRET_ONLY, // Only interpret; do not compile at REPL.
        HYBRID,         // Interpret first, fall back to compile (default).
    };

    // REPL loop control status returned by handle_repl_string().
    enum class ReplStatus {
        OK,          // Continue the loop.
        WANT_EXIT,   // User requested exit.
        WANT_RELOAD, // Reload the compiler (rebuild state).
        ERR,         // Recoverable error; keep the loop going.
    };

    // Per-function metadata registered with the compiler.
    //
    //   signature   — full TypeSpec, e.g. (function string int object)
    //   is_native   — true if this is a native C function (CallFf), false for script
    //   is_varargs  — true if the signature contains _varargs_ (or &rest)
    struct FunctionInfo {
        TypeSpec signature;
        bool     is_native = false;
        bool     is_varargs = false;
    };

    // Options passed to the Compiler constructor.
    // All target-specific knobs live here so the compiler stays the same
    // regardless of the platform it is configured for.
    struct CompilationOptions {
        SootPlatform             platform = SootPlatform::Default;
        CompilerMode             mode = CompilerMode::HYBRID;
        std::string              user_profile = "#f";
        bool                     debug_print_ir = false;
        bool                     debug_print_ast = false;
        bool                     debug_print_asm = false;
        std::vector<std::string> search_paths;
    };

    class Compiler {
    public:
        Compiler(const CompilationOptions          comp_options,
                 const std::optional<REPL::Config> repl_config = {},
                 const std::string                &user_profile = "#f",
                 std::unique_ptr<REPL::Wrapper>    repl = nullptr);

        ~Compiler();

        // --- Compilation ---
        std::expected<std::unique_ptr<BinaryFile>, std::string>
        compile_file(const std::filesystem::path &path);

        std::expected<std::unique_ptr<BinaryFile>, std::string>
        compile_file(soot::Object &forms, const std::string &filename);

        // --- REPL ---
        ReplStatus  handle_repl_string(const std::string &input);
        ReplStatus  handle_repl_command(const std::string &input);
        void        save_repl_history();
        void        print_to_repl(const std::string &str);
        std::string get_prompt();
        std::string get_repl_input();

        // --- Interpretation ---
        soot::Object interpret(const std::string &input);
        soot::Object interpret(const soot::Object &forms);

        // --- Types ---
        TypeSystem       &ts() { return m_ts; }
        const TypeSystem &ts() const { return m_ts; }

        TypeSpec parse_typespec(const soot::Object &form) { return ::parse_typespec(&m_ts, form); };

        // --- Function declarations ---
        //
        // define_function_signature   — for script lambdas (defun, defmethod).
        // define_native_signature     — for native C functions (define-extern).
        //
        // Both go into the same m_functions table; the `is_native` flag
        // decides whether CallNode emits `Call` or `CallFf`.
        void define_function_signature(const std::string &name, const TypeSpec &sig) {
            m_functions[name] = {sig, /*is_native=*/false, /*is_varargs=*/false};
        }

        void define_native_signature(const std::string &name, const TypeSpec &sig,
                                     bool is_varargs = false) {
            m_functions[name] = {sig, /*is_native=*/true, is_varargs};
        }

        std::optional<TypeSpec> lookup_function_signature(const std::string &name) const {
            auto it = m_functions.find(name);
            return it != m_functions.end() ? std::optional(it->second.signature) : std::nullopt;
        }

        bool is_native_function(const std::string &name) const {
            auto it = m_functions.find(name);
            return it != m_functions.end() && it->second.is_native;
        }

        bool is_varargs_function(const std::string &name) const {
            auto it = m_functions.find(name);
            return it != m_functions.end() && it->second.is_varargs;
        }

        // --- Environment management ---
        void         set_global(const std::string &name, const soot::Object &value);
        soot::Object get_global(const std::string &name);
        void         reload_environment();

        soot::Interpreter &get_soot_interpreter() { return m_soot; }
        soot::Object       get_soot_environment() { return m_soot.get_soot_environment(); }

        // --- Constants ---
        void define_constant(const std::string &name, const soot::Object &value) {
            m_constants[name] = value;
        }

        std::optional<soot::Object> lookup_constant(const std::string &name) const {
            auto it = m_constants.find(name);
            return it != m_constants.end() ? std::optional(it->second) : std::nullopt;
        }

        // --- Printing / saving ---
        void print_listing(const BinaryFile &file);
        bool save_binary(const BinaryFile &file, const std::filesystem::path &target_dir);
        bool save_listing(const BinaryFile &file, const std::filesystem::path &target_dir);

        // --- Macros ---
        bool         is_soot_macro(const std::string &name);
        soot::Object expand_soot_macro(const soot::Object &form);

        // --- REPL callbacks ---
        replxx::Replxx::completions_t
        find_symbols_or_object_file_by_prefix(const std::string &context, int &context_len,
                                              const std::vector<std::string> &examples);

        replxx::Replxx::hints_t find_hints_by_prefix(const std::string &context, int &context_len,
                                                     replxx::Replxx::Color          &color,
                                                     const std::vector<std::string> &examples);

        void repl_coloring(const std::string &input, replxx::Replxx::colors_t &colors);

        // --- Built-in SOOT forms ---
        soot::Object builtin_get_enum_vals(const soot::Object &form, soot::Arguments &args,
                                           const std::shared_ptr<soot::EnvironmentObject> &env);

        soot::Arguments get_va(const soot::Object &form, const soot::Object &rest);
        soot::Arguments get_va_no_named(const soot::Object &form, const soot::Object &rest);

        void va_check(
            const soot::Object &form, const soot::Arguments &args,
            const std::vector<std::optional<soot::ObjectType>> &unnamed,
            const std::unordered_map<std::string, std::pair<bool, std::optional<soot::ObjectType>>>
                &named);

        void for_each_in_list(const soot::Object                              &list,
                              const std::function<void(const soot::Object &)> &f);

        CompilerError make_error(const soot::Object &form, std::string where) const {
            CompilerError err(std::move(where));
            auto          info_str = m_soot.get_reader().get_db().get_info_for(form);
            if (info_str != "?") {
                if (info_str.starts_with("  at ")) { info_str = info_str.substr(5); }
                err.at(info_str);
            }
            return err;
        }

    private:
        // --- Initialization ---
        void load_soot_prelude(); // lib.sot -> m_soot (interpreted)
        void load_soc_prelude();  // lib.soc -> compiler (compiled)

        void load_user_profile();
        void setup_repl();
        void setup_soot_forms();

        // --- Compilation ---
        std::expected<std::unique_ptr<BinaryFile>, std::string>
             compile_internal(soot::Object &forms, const std::string &filename);
        void render_internal_error(const std::exception &e);

        // --- Helpers ---
        ReplStatus interpret_and_print(const std::string &script);
        ReplStatus compile_and_report(const std::string &code);
        ReplStatus try_interpret_then_compile(const std::string &code);

        void                 color_binary_file(std::unique_ptr<BinaryFile> &binary);
        std::vector<uint8_t> codegen_binary(BinaryFile *binary);

        std::string find_file(const std::string &filename);
        std::string read_file_content(const std::string &filename);

        void print_error(const std::string &context, const std::exception &e);
        void print_warning(const std::string &warning);

        // Report a compilation error with a formatted message.
        template <typename... Args>
        [[noreturn]] void throw_compiler_error(const soot::Object &code, const std::string &str,
                                               Args &&...args) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "-- Compilation Error! --\n");
            if (!str.empty() && str.back() == '\n') {
                fmt::print(fmt::emphasis::bold, fmt::runtime(str), std::forward<Args>(args)...);
            } else {
                fmt::print(fmt::emphasis::bold, fmt::runtime(str + '\n'),
                           std::forward<Args>(args)...);
            }
            fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Form:\n");
            fmt::print("{}\n", code.print());
            throw CompilerError("Compilation Error");
        }

        // --- State ---
        TypeSystem                    &m_ts;
        SootPlatform                   m_platform;
        soot::Interpreter              m_soot;
        std::unique_ptr<REPL::Wrapper> m_repl;

        CompilationOptions          m_config;
        std::unique_ptr<GlobalNode> m_global_env;
        std::unique_ptr<NoneNode>   m_none;

        std::string                                   m_current_file;
        std::unordered_map<std::string, FunctionInfo> m_functions;
        std::unordered_map<std::string, soot::Object> m_constants;
    };

} // namespace sootc