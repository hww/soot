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
#include "sootc/runtime/Runtime.hpp"

#include "sootc/node/GlobalNode.hpp"
#include "sootc/node/Node.hpp"
#include "sootc/node/NoneNode.hpp"

#include "script/Interpreter.hpp"
#include "script/Object.hpp"
#include "script/Reader.hpp"

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
        bool                     print_listing = false;
        std::string              log_level = "info";
        std::string              sidbase_path;
        std::vector<std::string> search_paths;

        /// @brief Execution backend.
        ///
        /// @details Empty string        -> LocalVmRuntime (default).
        ///          "null"             -> NullRuntime (for --compile-only
        ///                                and tests).
        ///          "<host>:<port>"    -> RemoteRuntime (talk to an
        ///                                external carbon.exe over the
        ///                                cross-socket protocol).
        ///
        ///          Do not include a scheme prefix; the runtime parses
        ///          optional "tcp://" or "http://" if present.
        std::string runtime_url;


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
        compile_file(script::Object &forms, const std::string &filename);

        // --- Require (language form) ---
        //
        // Implements (require "path/to/file.sc") as a LANGUAGE form.
        // The compiler dispatches this from NodeBuilder when it sees
        // `require` in a source file: the file is compiled and the
        // resulting module is registered in the active runtime, all at
        // compile time. The form itself produces no IR.
        //
        // The path is resolved against the compiler's search_paths. If
        // the file has already been loaded in this process (via a
        // previous require, or manually by the user), the call is a
        // no-op. This makes diamond-shaped dependency graphs cheap: a
        // file required by two of its dependents is compiled only once.
        //
        // Cyclic requires are not supported: a file that requires
        // itself, directly or transitively, is silently skipped on the
        // second visit (the entry is added to m_loaded_files BEFORE
        // the recursive compile, so the recursion terminates).
        //
        // @param form    The (require "...") form, used only for error
        //                messages.
        // @param path    The path string from the form.
        // @return        true if the file was loaded (or was already
        //                loaded before this call).
        bool        require_file(const script::Object &form, const std::string &path);

        // --- REPL ---
        ReplStatus  handle_repl_string(const std::string &input);
        void        save_repl_history();
        void        print_to_repl(const std::string &str);
        std::string get_prompt();
        std::string get_repl_input();

        // --- Interpretation ---
        script::Object interpret(const std::string &input);
        script::Object interpret(const script::Object &forms);

        // --- Types ---
        TypeSystem       &ts() { return m_ts; }
        const TypeSystem &ts() const { return m_ts; }

        TypeSpec parse_typespec(const script::Object &form) { return ::parse_typespec(&m_ts, form); };

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
        void         set_global(const std::string &name, const script::Object &value);
        script::Object get_global(const std::string &name);
        void         reload_environment();

        script::Interpreter &get_soot_interpreter() { return m_soot; }
        script::Object       get_soot_environment() { return m_soot.get_soot_environment(); }

        // --- Constants ---
        void define_constant(const std::string &name, const script::Object &value) {
            m_constants[name] = value;
        }

        std::optional<script::Object> lookup_constant(const std::string &name) const {
            auto it = m_constants.find(name);
            return it != m_constants.end() ? std::optional(it->second) : std::nullopt;
        }

        // --- Printing / saving ---
        void print_listing(const BinaryFile &file);
        bool save_binary(const BinaryFile &file, const std::filesystem::path &target_dir);
        bool save_listing(const BinaryFile &file, const std::filesystem::path &target_dir);
        bool save_header(const BinaryFile &file, const std::filesystem::path &target_dir);
        /// @brief Save the whole REPL session to <name>.bin and <name>.h.
        /// @param name   Base name (without extension). Defaults to "repl".
        /// @param dir    Output directory. Defaults to ".".
        /// @return true on success.
        bool save_session(const std::string &name, const std::filesystem::path &dir);

        // --- Macros ---
        bool         is_soot_macro(const std::string &name);
        script::Object expand_soot_macro(const script::Object &form);

        // --- Lambda name synthesis ---
        //
        // NodeBuilder calls this when it encounters a call whose head is
        // not a symbol, e.g. ((lambda (x) x) 5). The lambda is given a
        // unique name so that the enclosing FileNode can register it in
        // its symbol table and the CallNode that replaces the original
        // form can reference it by that name.
        //
        // The name is unique within this Compiler instance, not globally,
        // so separate Compiler instances (and tests) do not interfere
        // with one another.
        //
        // Format: "<base>::lambda#<n>", where <n> is a monotonically
        // increasing counter starting at 0.
        //
        // @param base  Human-readable prefix, typically the enclosing
        //              file's name. May be empty.
        // @return      A name that has not been produced by this
        //              Compiler before.
        std::string make_unique_lambda_name(const std::string &base);

        // --- Runtime ---
        /// @brief Access the active runtime.
        Runtime       &runtime() { return *m_runtime; }
        const Runtime &runtime() const { return *m_runtime; }

        // --- REPL callbacks ---
        replxx::Replxx::completions_t
        find_symbols_or_object_file_by_prefix(const std::string &context, int &context_len,
                                              const std::vector<std::string> &examples);

        replxx::Replxx::hints_t find_hints_by_prefix(const std::string &context, int &context_len,
                                                     replxx::Replxx::Color          &color,
                                                     const std::vector<std::string> &examples);

        void repl_coloring(const std::string &input, replxx::Replxx::colors_t &colors);

        // --- Built-in SOOT forms ---
        script::Object builtin_get_enum_vals(const script::Object &form, script::Arguments &args,
                                           const std::shared_ptr<script::EnvironmentObject> &env);

        script::Arguments get_va(const script::Object &form, const script::Object &rest);
        script::Arguments get_va_no_named(const script::Object &form, const script::Object &rest);

        void va_check(
            const script::Object &form, const script::Arguments &args,
            const std::vector<std::optional<script::ObjectType>> &unnamed,
            const std::unordered_map<std::string, std::pair<bool, std::optional<script::ObjectType>>>
                &named);

        void for_each_in_list(const script::Object                              &list,
                              const std::function<void(const script::Object &)> &f);

        CompilerError make_error(const script::Object &form, std::string where) const {
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
        void load_soot_prelude(); // lib.st -> m_soot (interpreted)
        void load_soc_prelude();  // lib.sc -> compiler (compiled)

        void load_user_profile();
        void setup_repl();
        void setup_soot_forms();

        // --- Compilation ---
        std::expected<std::unique_ptr<BinaryFile>, std::string>
             compile_internal(script::Object &forms, const std::string &filename);
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
        [[noreturn]] void throw_compiler_error(const script::Object &code, const std::string &str,
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

        // ------------------------------------------------------------------
        // REPL command dispatch
        // ------------------------------------------------------------------
        //
        // Commands are recognised by handle_repl_string BEFORE the normal
        // compile pipeline runs. A line is a command if, when parsed as a
        // single SOOT form, it is a proper list whose head is a symbol in
        // repl_commands(). Command forms never reach NodeBuilder, never
        // produce IR, and never get registered in Globals.
        //
        // This separation is deliberate: REPL commands are tools of the
        // compiler, not forms of the language. They must not shadow or be
        // shadowed by built-in forms, and they must not appear in the
        // compiled output.

        /// @brief The set of names recognised as REPL commands.
        static const std::set<std::string> &repl_commands();

        /// @brief If `input` parses to a REPL command form, dispatch it.
        /// @return The command's ReplStatus, or std::nullopt if `input`
        ///         is not a REPL command and should go through the
        ///         normal compile pipeline.
        std::optional<ReplStatus> try_handle_repl_command(const std::string &input);

        /// @brief Map a command name to its handler.
        ReplStatus dispatch_repl_command(const std::string &name, const script::Object &form);

        // Individual command handlers. Each returns ReplStatus so that a
        // command can request exit / reload; malformed arguments print
        // "; ERROR: ..." and return ReplStatus::ERR.
        ReplStatus cmd_help(const script::Object &form);
        ReplStatus cmd_clear(const script::Object &form);
        ReplStatus cmd_e(const script::Object &form);
        ReplStatus cmd_reload(const script::Object &form);
        ReplStatus cmd_sizes(const script::Object &form);
        ReplStatus cmd_inspect_env(const script::Object &form);
        ReplStatus cmd_inspect_env_soots(const script::Object &form);
        ReplStatus cmd_has_macro(const script::Object &form);
        ReplStatus cmd_list_symbols(const script::Object &form);
        ReplStatus cmd_save_session(const script::Object &form);
        ReplStatus cmd_sc_run(const script::Object &form);
        ReplStatus cmd_sc_eval(const script::Object &form);

        // --- Arguments ---
        ReplStatus cmd_arg_set(const script::Object &form);
        ReplStatus cmd_arg_get(const script::Object &form);

        // --- State ---
        TypeSystem                    &m_ts;
        SootPlatform                   m_platform;
        script::Interpreter              m_soot;
        std::unique_ptr<REPL::Wrapper> m_repl;

        CompilationOptions          m_config;
        std::unique_ptr<GlobalNode> m_global_env;
        std::unique_ptr<NoneNode>   m_none;

        std::string                                   m_current_file;
        std::unordered_map<std::string, FunctionInfo> m_functions;
        std::unordered_map<std::string, script::Object> m_constants;

        // Per-instance set of already-loaded prelude paths.
        //
        // MUST NOT be static: each Compiler instance has its own interpreter
        // (`m_soot`), and macro registration is per-interpreter. A static
        // set would silently skip loading lib.sc into the second and later
        // Compiler instances, leaving them without `defun`, `let`, etc.,
        // which breaks every unit test that creates a fresh Compiler.
        std::set<std::filesystem::path> m_loaded_soot_libs;
        std::set<std::filesystem::path> m_loaded_soc_libs;

        /// @brief All top-level forms entered in the REPL during this session.
        /// @details Appended to on every successful compile_and_report().
        ///          Used by :save / :cf to write the whole session to disk.
        std::vector<script::Object> m_session_forms;

        /// @brief Base name used when saving the REPL session.
        std::string m_session_name = "repl";

        /// @brief Monotonic counter used by make_unique_lambda_name.
        ///
        /// @details NodeBuilder synthesizes a name for every anonymous
        ///          lambda that appears in the head of a call, e.g.
        ///          ((lambda (x) x) 5). The counter is per-Compiler so
        ///          that separate Compiler instances — including the
        ///          ones constructed by individual tests — never collide.
        ///
        ///          The value is intentionally not reset by anything: it
        ///          only ever grows, which is what "unique within this
        ///          Compiler" means.
        u64 m_lambda_counter = 0;

        /// @brief Set to true by the (e) custom form to request REPL exit.
        /// @details handle_repl_string reads this after running a form and
        ///          converts it into ReplStatus::WANT_EXIT.
        bool m_want_exit = false;

        /// @brief Set to true by the (reload) custom form to request a
        ///        compiler reload.
        /// @details handle_repl_string reads this after running a form and
        ///          converts it into ReplStatus::WANT_RELOAD.
        bool m_want_reload = false;

        /// @brief The execution backend.
        ///
        /// @details Decided once in the constructor from
        ///          `CompilationOptions::runtime_url`, and used for
        ///          everything that used to talk directly to
        ///          `carbon::Globals` or `carbon::VirtualMachine`:
        ///
        ///            * compile_and_report — to load a module.
        ///            * cmd_sc_run         — to call a function.
        ///
        ///          The default (empty runtime_url) is a local
        ///          in-process VM, which is what sootc has always
        ///          used. A non-empty runtime_url means "talk to an
        ///          external host" (RemoteRuntime), or "don't run
        ///          anything" (NullRuntime) when the url is "null".
        std::unique_ptr<Runtime> m_runtime;

        // --- Loaded files (for require) ---
        //
        // Absolute paths of .sc files that have already been loaded
        // in this Compiler instance. Populated by require_file(). Used
        // to make requires idempotent, so a file that is required by
        // two of its dependents is compiled only once.
        std::set<std::filesystem::path> m_loaded_files;
    };
} // namespace sootc