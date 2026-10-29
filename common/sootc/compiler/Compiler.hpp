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
#include "sootc/compiler/ModuleRef.hpp"
#include "sootc/compiler/NodeBuilder.hpp"

#include "sootc/runtime/Runtime.hpp"

#include "sootc/node/GlobalNode.hpp"
#include "sootc/node/Node.hpp"
#include "sootc/node/NoneNode.hpp"

#include "script/Interpreter.hpp"
#include "script/Object.hpp"
#include "script/Reader.hpp"

#include "carbon/vm/VirtualMachine.hpp"

#include "carbon/file/BinaryFile.hpp"
#include "carbon/file/Globals.hpp"
#include "type_system/Deftype.hpp"
#include "type_system/TypeSystem.hpp"

namespace sootc {

    class NodeBuilder;
    class StateScriptNode;

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

        /// Project root. Set once at startup from --project-root
        /// (or the current working directory if not given). All
        /// relative module paths are resolved against this root.
        std::filesystem::path project_root;

        /// Output directory for compiled artifacts. Defaults to
        /// "build". Relative paths are resolved against project_root.
        std::filesystem::path target_dir = "build";

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

        /// @brief Compile forms as a regular file (not a REPL input).
        /// @details Sets is_repl = false in the underlying call. Used
        ///          by require_file, load_soc_prelude, and anything
        ///          that compiles a .sc file from disk.
        std::expected<std::unique_ptr<BinaryFile>, std::string>
        compile_file(script::Object &forms, const std::string &filename);

        /// @brief Compile a file by path. Reads, parses, and compiles.
        /// @details Convenience overload for file-based compilation.
        ///          Non-REPL: is_repl = false.
        std::expected<std::unique_ptr<BinaryFile>, std::string>
        compile_file(const std::filesystem::path &path);

        /// @brief Compile forms as a REPL input.
        /// @details Sets is_repl = true. Used only by compile_and_report.
        ///          A REPL input gets a unique logical name (see
        ///          compile_internal) so that consecutive inputs do not
        ///          unload each other via load_module_from_binary's
        ///          same-name replacement.
        std::expected<std::unique_ptr<BinaryFile>, std::string>
        compile_file_repl(script::Object &forms, const std::string &filename);

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
        bool require_file(const script::Object &form, const std::string &path);

        /// @brief Resolve a require path to a ModuleRef.
        /// @details Resolution rules:
        ///            1. "./foo" or "../foo" — relative to the
        ///               directory of `from_id`.
        ///            2. anything else — relative to project_root.
        ///          Absolute paths are rejected: they would let a
        ///          module depend on an arbitrary file outside the
        ///          project, breaking reproducibility.
        ///          search_paths is NOT consulted here.
        std::optional<ModuleRef> resolve_require(const std::string &path,
                                                 const ModuleId    &from_id) const;

        /// @brief Resolve a module path for save_module / save_module_recursive.
        /// @details Same as resolve_require, but also accepts an
        ///          absolute path (which is converted to a ModuleId by
        ///          making it relative to project_root).
        std::optional<ModuleRef> resolve_module(const std::string &path) const;

        /// @brief Compile a .sc file and every module it requires,
        ///        saving all of them as .bin + .h.
        /// @details Dependencies are saved first, then the file
        ///          itself, matching load_module_recursive's order.
        ///          Cyclic requires are detected and reported as an
        ///          error.
        bool save_module_recursive(const std::filesystem::path &source_file,
                                   const std::filesystem::path &out_dir = "build");

        /// @brief Look up the FileNode by its ModuleId.
        /// @details Returns nullptr if the module has not been compiled
        ///          in this Compiler instance.
        FileNode *find_file_node(const ModuleId &id) const {
            auto it = m_file_nodes.find(id);
            return it != m_file_nodes.end() ? it->second.get() : nullptr;
        }

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

        TypeSpec parse_typespec(const script::Object &form) {
            return ::parse_typespec(&m_ts, form);
        };

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
        void           set_global(const std::string &name, const script::Object &value);
        script::Object get_global(const std::string &name);
        void           reload_environment();

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
        /// @brief Compile a .sc file and save it as its own module (.bin + .h).
        ///
        /// @details Unlike save_session, which serialises everything entered in
        ///          the REPL as one throwaway module, this method compiles ONE
        ///          source file and writes exactly what that file contains —
        ///          its own declarations and its own (require ...) list.
        ///
        ///          The module's name is the file stem. A module never contains
        ///          a dependency on itself: if the file contains
        ///          (require "bar.sc") and the file is bar.sc, the dependency is
        ///          dropped (see NodeBuilder::build_require).
        ///
        ///          The output goes to <out_dir>/<stem>.bin and
        ///          <out_dir>/<stem>.h. The directory is resolved against
        ///          PROJECT if relative, same as save_binary/save_header.
        ///
        /// @param source_file  Path to the .sc file. Resolved against
        ///                     m_config.search_paths if relative.
        /// @param out_dir      Output directory. Defaults to "build".
        /// @return true on success.
        bool           save_module(const std::filesystem::path &source_file,
                                   const std::filesystem::path &out_dir = "build");
        /// @brief Save the accumulated SIDBase to the path from m_config.
        /// @details Called by main.cpp after all input files are compiled,
        ///          or on REPL exit. This is the ONLY place that writes
        ///          sidbase to disk.
        void save_sidbase();
        // --- Macros ---
        bool is_soot_macro(const std::string &name);
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

        void va_check(const script::Object &form, const script::Arguments &args,
                      const std::vector<std::optional<script::ObjectType>> &unnamed,
                      const std::unordered_map<
                          std::string, std::pair<bool, std::optional<script::ObjectType>>> &named);

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
        /// @brief The core compilation driver.
        ///
        /// @param forms     Top-level forms of the input.
        /// @param filename  Display name and file key.
        /// @param is_repl   True when this input comes from the REPL.
        ///                  Controls two things:
        ///                    * The module's logical name is made unique
        ///                      ("<repl-N>") so that subsequent REPL
        ///                      inputs do not unload it via
        ///                      load_module_from_binary's same-name
        ///                      replacement.
        ///                    * Redefinition of an existing name is
        ///                      allowed: the old child is removed
        ///                      from m_children before the new one is
        ///                      bound.
        ///
        ///                  For non-REPL files, both are false.
        ///
        ///                  There is no per-file export list: every
        ///                  top-level name is registered in Globals,
        ///                  so a later input can resolve it by name
        ///                  regardless of which input defined it.
        std::expected<std::unique_ptr<BinaryFile>, std::string>
        compile_internal(script::Object &forms, const std::string &filename, bool is_repl);

        void render_internal_error(const std::exception &e);

        /// @brief Compile every state handler body and every track step
        ///        of a StateScriptNode into FunctionNode children of
        ///        `file`, so that FileNode::collect_all picks them up
        ///        and assigns them file offsets.
        ///
        /// @details Called from compile_internal after all top-level
        ///          forms have been processed and before file->generate().
        ///
        ///          Naming convention (must match build_state_script):
        ///            <type>-<state>-<kind>            for enter/exit/...
        ///            <type>-<state>-event-<event-id>  for events
        ///            <type>-<state>-<kind>-track-<track>-main-<i>     for main steps
        ///            <type>-<state>-<kind>-track-<track>-frame-<N>    for frame steps
        void compile_state_script_bodies(StateScriptNode *ss, FileNode *file, NodeBuilder &builder);

        // --- Helpers ---
        ReplStatus interpret_and_print(const std::string &script);
        ReplStatus compile_and_report(script::Object forms);
        ReplStatus try_interpret_then_compile(script::Object form);

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
        // Config
        // ------------------------------------------------------------------
    public:
        CompilationOptions *config() { return &m_config; }

        // ------------------------------------------------------------------
        // Globals
        // ------------------------------------------------------------------
    public:
        GlobalNode       *global_env() { return m_global_env.get(); }
        const GlobalNode *global_env() const { return m_global_env.get(); }

        // ------------------------------------------------------------------
        // REPL command dispatch
        // ------------------------------------------------------------------
    private:
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
        std::optional<ReplStatus> try_handle_repl_command(const script::Object &form);

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
        ReplStatus cmd_save_module(const script::Object &form);
        ReplStatus cmd_save_module_recursive(const script::Object &form);
        ReplStatus cmd_sc_run(const script::Object &form);
        ReplStatus cmd_sc_eval(const script::Object &form);

        // --- Arguments ---
        ReplStatus cmd_arg_set(const script::Object &form);
        ReplStatus cmd_arg_get(const script::Object &form);
        ReplStatus cmd_arg_list(const script::Object &form);

        // --- Garbage collector ---
        ReplStatus cmd_gc_list(const script::Object &form);
        ReplStatus cmd_gc_collect(const script::Object &form);

        // --- Type system ---
        ReplStatus cmd_ts_info(const script::Object &form);

        // --- AST ---
        ReplStatus cmd_ast(const script::Object &form);

        // --- State ---
        TypeSystem                    &m_ts;
        SootPlatform                   m_platform;
        script::Interpreter            m_soot;
        std::unique_ptr<REPL::Wrapper> m_repl;

        CompilationOptions          m_config;
        std::unique_ptr<GlobalNode> m_global_env;
        std::unique_ptr<NoneNode>   m_none;

        ModuleId                                        m_current_file;
        std::unordered_map<std::string, FunctionInfo>   m_functions;
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
        // Logical module ids that have already been loaded in this
        // Compiler instance. Populated by require_file(). Used to make
        // requires idempotent, so a file that is required by two of
        // its dependents is compiled only once.
        //
        // Keyed by ModuleId (relative to project_root), not by absolute
        // path: the module identity is stable across project moves.
        std::set<ModuleId> m_loaded_files;

        // ------------------------------------------------------------------
        // Loaded FileNodes
        // ------------------------------------------------------------------
        //
        // Every file that has been compiled in this session leaves its
        // FileNode here, keyed by the canonical path (or by the
        // synthetic name for REPL inputs). The FileNode owns the
        // symbol table of that file; `require` needs to keep it alive
        // so that the requirer's FileNode can point at it through
        // m_imports.
        //
        // The FileNode is destroyed only when the Compiler is destroyed.
        // Keyed by ModuleId (relative to project_root).
        std::unordered_map<ModuleId, std::unique_ptr<FileNode>> m_file_nodes;

        // ------------------------------------------------------------------
        // REPL File Counter
        // ------------------------------------------------------------------
        /// @brief Monotonic counter for REPL module names.
        ///
        /// @details Every REPL input is compiled as its own module with
        ///          a unique logical name "<repl-N>". This is what makes
        ///          Globals accumulate symbols across inputs instead of
        ///          replacing them: load_module_from_binary unloads any
        ///          existing module with the same logical name, so
        ///          reusing "<repl>" for every input would erase every
        ///          previous input's slots on each new input.
        u64 m_repl_counter = 0;

        // ------------------------------------------------------------------
        // Snapshot
        // ------------------------------------------------------------------
        /// @brief Снимок GlobalNode на время компиляции одного ввода.
        /// @details Снимается в начале compile_internal, откатывается
        ///          при исключении. Гарантирует, что global_env не
        ///          удержит висячий указатель после rollback FileNode.
        GlobalNode::Snapshot m_global_snapshot;
        // sootc/compiler/Compiler.hpp
    private:
        // Синхронизирует глобальный флаг Type::verbose с текущим
        // m_config.log_level. Вызывается при любом изменении log_level:
        // в конструкторе (после инициализации m_config) и в
        // cmd_arg_set при смене "log-level".
        //
        // "debug" -> Type::verbose = true, всё остальное -> false.
        // Trace намеренно не включает verbose: trace — это про VM/compiler
        // внутренности, а Type::verbose — про дампы полей типов.
        void sync_type_verbose();

    private:
        bool check_type_methods_implemented(const FileNode *file_node) const;

        bool save_recursive_impl(const ModuleRef &ref, const std::filesystem::path &out_dir,
                                 std::set<ModuleId> &visited, std::set<ModuleId> &active);
    };
} // namespace sootc