// sootc/compiler/Compiler.hpp
#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
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

#include "util/Config.hpp"

namespace sootc {

    class NodeBuilder;
    class StateScriptNode;

    // ================================================================
    // Compiler mode
    // ================================================================
    //
    // Selects what the REPL does with an input that is not a REPL
    // command: interpret it, compile it, or try both.
    //
    // The CLI exposes this as --mode compile|interpret|hybrid. The
    // enum is kept for code that needs to branch on it; the string
    // form in Config is the source of truth.
    enum class CompilerMode {
        COMPILE_ONLY,   // Only compile; do not interpret at REPL.
        INTERPRET_ONLY, // Only interpret; do not compile at REPL.
        HYBRID,         // Interpret first, fall back to compile (default).
    };

    // ================================================================
    // Input origin
    // ================================================================
    //
    // Where the current compilation input came from. Affects only
    // explicit ./ and ../ path resolution in require.
    enum class InputOrigin {
        Default, // Real .sc file on disk. base_dir is its directory.
        Test,    // Synthetic test input. base_dir is project_root.
        Repl,    // REPL input. base_dir is project_root.
    };

    // ================================================================
    // REPL loop control
    // ================================================================
    //
    // Returned by handle_repl_string() and by every cmd_* handler.
    enum class ReplStatus {
        OK,          // Continue the loop.
        WANT_EXIT,   // User requested exit.
        WANT_RELOAD, // Reload the compiler (rebuild state).
        ERR,         // Recoverable error; keep the loop going.
    };

    // ================================================================
    // Function metadata
    // ================================================================
    //
    // Per-function information registered with the compiler.
    //
    //   signature   — full TypeSpec, e.g. (function string int object)
    //   is_native   — true for native C functions (CallFf), false for script
    //   is_varargs  — true if the signature contains _varargs_ (or &rest)
    struct FunctionInfo {
        TypeSpec signature;
        bool     is_native = false;
        bool     is_varargs = false;
    };

    // ================================================================
    // CompilationOptions
    // ================================================================
    //
    // Deprecated. The Compiler now takes a common::Config; this
    // struct is kept only until the last caller (tests, save_module
    // internal path) is migrated. Do not add fields here.
    //
    // TODO: delete once every caller constructs the Compiler from
    //       common::Config.
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
        std::filesystem::path    project_root;
        std::filesystem::path    target_dir = "build";
        std::string              runtime_url;
    };

    // ================================================================
    // Compiler
    // ================================================================
    //
    // One Compiler per process (or per test). It owns the TypeSystem
    // reference, the SOOT interpreter, the global environment, the
    // runtime backend, and the per-session state.
    //
    // Configuration comes from common::Config, held by reference.
    // The caller (main, or a test) must keep the Config alive for the
    // lifetime of the Compiler.
    class Compiler {
    public:
        // ----- Lifecycle -----

        // @brief Construct the compiler.
        //
        // @param cfg          The single source of truth for every
        //                     option. Must outlive the Compiler.
        // @param repl_config  Optional REPL shell configuration. Not
        //                     part of cfg: it describes the
        //                     interactive shell, not the compiler.
        // @param user_profile Profile name, kept for REPL::Wrapper.
        //                     Same value as cfg.compiler.user_profile.
        // @param repl         Optional REPL wrapper.
        Compiler(const common::Config &cfg, const std::optional<REPL::Config> repl_config = {},
                 const std::string             &user_profile = "#f",
                 std::unique_ptr<REPL::Wrapper> repl = nullptr);

        ~Compiler();

        // ----- Configuration -----

        // @brief The configuration this compiler was built from.
        const common::Config &config() const { return m_config; }

        // ----- Global environment -----

        GlobalNode       *global_env() { return m_global_env.get(); }
        const GlobalNode *global_env() const { return m_global_env.get(); }

        // ----- Compilation -----

        // @brief Mark the next compile_file() input as a test input.
        //
        // @details Affects only explicit ./ and ../ path resolution
        //          in require: they will be resolved against
        //          project_root instead of the (non-existent) parent
        //          directory of the synthetic name.
        void set_input_origin(InputOrigin origin) { m_input_origin = origin; }

        // @brief Where the current compilation input came from.
        //
        // @details Used by NodeBuilder::build_call to decide whether
        //          an unresolved script function may fall back to the
        //          flat m_functions table (REPL and test inputs) or
        //          must be visible through FileNode::lookup (regular
        //          files).
        InputOrigin input_origin() const { return m_input_origin; }

        // @brief Compile forms as a regular file (not a REPL input).
        //
        // @details Sets is_repl = false in the underlying call. Used
        //          by require_file, load_soc_bootstrap, and anything
        //          that compiles a .sc file from disk.
        std::expected<std::unique_ptr<BinaryFile>, std::string>
        compile_file(script::Object &forms, const std::string &filename);

        // @brief Compile a file by path. Reads, parses, and compiles.
        std::expected<std::unique_ptr<BinaryFile>, std::string>
        compile_file(const std::filesystem::path &path);

        // @brief Compile forms as a REPL input.
        //
        // @details Sets is_repl = true. Used only by compile_and_report.
        //          A REPL input gets a unique logical name (see
        //          compile_internal) so that consecutive inputs do not
        //          unload each other.
        std::expected<std::unique_ptr<BinaryFile>, std::string>
        compile_file_repl(script::Object &forms, const std::string &filename);

        // ----- Require -----

        // @brief Implement (require "path/to/file.sc") as a language form.
        //
        // @details Compiles the required file (if not already loaded)
        //          and registers the resulting module in the runtime.
        //          Adds an import edge from the requirer's FileNode to
        //          the required module's FileNode, so that
        //          FileNode::lookup can resolve names across files.
        //
        //          Idempotent: a module already loaded in this Compiler
        //          is a no-op (but the import edge is still added).
        //          Cyclic requires terminate on the second visit.
        bool require_file(const script::Object &form, const std::string &path);

        // @brief Resolve a require path to a ModuleRef.
        //
        // @details Resolution rules:
        //            1. "./foo" or "../foo" — relative to the
        //               directory of `from_id`, or to project_root
        //               for Test/Repl inputs.
        //            2. anything else — relative to project_root.
        //          Absolute paths are rejected.
        std::optional<ModuleRef> resolve_require(const std::string &path,
                                                 const ModuleId    &from_id) const;

        // @brief Resolve a module path for save_module / save_module_recursive.
        //
        // @details Same as resolve_require, but also accepts an
        //          absolute path (converted to a ModuleId relative to
        //          project_root).
        std::optional<ModuleRef> resolve_module(const std::string &path) const;

        // @brief Compile a .sc file and every module it requires,
        //        saving all of them as .bin + .h.
        bool save_module_recursive(const std::filesystem::path &source_file,
                                   const std::filesystem::path &out_dir = "build");

        // @brief Look up the FileNode by its ModuleId.
        FileNode *find_file_node(const ModuleId &id) const {
            auto it = m_file_nodes.find(id);
            return it != m_file_nodes.end() ? it->second.get() : nullptr;
        }

        // ----- REPL -----

        ReplStatus  handle_repl_string(const std::string &input);
        void        save_repl_history();
        void        print_to_repl(const std::string &str);
        std::string get_prompt();
        std::string get_repl_input();

        // ----- Interpretation -----

        script::Object interpret(const std::string &input);
        script::Object interpret(const script::Object &forms);

        // ----- Types -----

        TypeSystem       &ts() { return m_ts; }
        const TypeSystem &ts() const { return m_ts; }

        TypeSpec parse_typespec(const script::Object &form) {
            return ::parse_typespec(&m_ts, form);
        }

        // ----- Function declarations -----

        void define_function_signature(const std::string &name, const TypeSpec &sig) {
            m_native_functions[name] = {sig, /*is_native=*/false, /*is_varargs=*/false};
        }

        void define_native_signature(const std::string &name, const TypeSpec &sig,
                                     bool is_varargs = false) {
            m_native_functions[name] = {sig, /*is_native=*/true, is_varargs};
        }

        std::optional<TypeSpec> lookup_function_signature(const std::string &name) const {
            auto it = m_native_functions.find(name);
            return it != m_native_functions.end() ? std::optional(it->second.signature)
                                                  : std::nullopt;
        }

        bool is_native_function(const std::string &name) const {
            auto it = m_native_functions.find(name);
            return it != m_native_functions.end() && it->second.is_native;
        }

        bool is_varargs_function(const std::string &name) const {
            auto it = m_native_functions.find(name);
            return it != m_native_functions.end() && it->second.is_varargs;
        }

        // ----- Environment -----

        void           set_global(const std::string &name, const script::Object &value);
        script::Object get_global(const std::string &name);
        void           reload_environment();

        script::Interpreter &get_soot_interpreter() { return m_soot; }
        script::Object       get_soot_environment() { return m_soot.get_soot_environment(); }

        // ----- Constants -----

        void define_constant(const std::string &name, const script::Object &value) {
            m_constants[name] = value;
        }

        std::optional<script::Object> lookup_constant(const std::string &name) const {
            auto it = m_constants.find(name);
            return it != m_constants.end() ? std::optional(it->second) : std::nullopt;
        }

        // ----- Printing / saving -----

        void print_listing(const BinaryFile &file);
        bool save_binary(const BinaryFile &file, const std::filesystem::path &target_dir);
        bool save_listing(const BinaryFile &file, const std::filesystem::path &target_dir);
        bool save_header(const BinaryFile &file, const std::filesystem::path &target_dir);

        // @brief Save the whole REPL session to <name>.bin and <name>.h.
        bool save_session(const std::string &name, const std::filesystem::path &dir);

        // @brief Compile a .sc file and save it as its own module.
        bool save_module(const std::filesystem::path &source_file,
                         const std::filesystem::path &out_dir = "build");

        // @brief Save the accumulated SIDBase to the path from Config.
        void save_sidbase();

        // ----- Macros -----

        bool           is_soot_macro(const std::string &name);
        script::Object expand_soot_macro(const script::Object &form);

        // ----- Lambda names -----

        // @brief Synthesize a unique name for an anonymous lambda.
        std::string make_unique_lambda_name(const std::string &base);

        // ----- Runtime -----

        Runtime       &runtime() { return *m_runtime; }
        const Runtime &runtime() const { return *m_runtime; }

        // ----- REPL callbacks -----

        replxx::Replxx::completions_t
        find_symbols_or_object_file_by_prefix(const std::string &context, int &context_len,
                                              const std::vector<std::string> &examples);

        replxx::Replxx::hints_t find_hints_by_prefix(const std::string &context, int &context_len,
                                                     replxx::Replxx::Color          &color,
                                                     const std::vector<std::string> &examples);

        void repl_coloring(const std::string &input, replxx::Replxx::colors_t &colors);

        // ----- Built-in SOOT forms -----

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


        // ----- REPL command handlers -----
        //
        // Declared here, defined in ReplCommands.cpp.

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
        ReplStatus cmd_arg_set(const script::Object &form);
        ReplStatus cmd_arg_get(const script::Object &form);
        ReplStatus cmd_arg_list(const script::Object &form);
        ReplStatus cmd_gc_list(const script::Object &form);
        ReplStatus cmd_gc_collect(const script::Object &form);
        ReplStatus cmd_ts_info(const script::Object &form);
        ReplStatus cmd_ast(const script::Object &form);

    private:
        // ================================================================
        // Compilation internals
        // ================================================================

        // @brief Core compilation driver.
        //
        // @param forms     Top-level forms of the input.
        // @param filename  Display name and file key.
        // @param is_repl   True when this input comes from the REPL.
        //                  Controls two things:
        //                    * The module's logical name is made unique
        //                      ("<repl-N>") so that subsequent REPL
        //                      inputs do not unload it via
        //                      load_module_from_binary's same-name
        //                      replacement.
        //                    * Redefinition of an existing name is
        //                      allowed: the old child is removed
        //                      from m_children before the new one is
        //                      bound.
        //
        //                  For non-REPL files, both are false.
        //
        //                  There is no per-file export list: every
        //                  top-level name is registered in Globals,
        //                  so a later input can resolve it by name
        //                  regardless of which input defined it.
        std::expected<std::unique_ptr<BinaryFile>, std::string>
        compile_internal(script::Object &forms, const std::string &filename, bool is_repl);

        void render_internal_error(const std::exception &e);

        // @brief Compile state-script handler bodies and track steps
        //        into FunctionNodes owned by `file`.
        //
        // @details Called from compile_internal after all top-level
        //          forms have been processed and before
        //          file->generate(). Naming convention (must match
        //          build_state_script):
        //            <type>-<state>-<kind>            for enter/exit/...
        //            <type>-<state>-event-<event-id>  for events
        //            <type>-<state>-<kind>-track-<track>-main-<i>     for main steps
        //            <type>-<state>-<kind>-track-<track>-frame-<N>    for frame steps
        void compile_state_script_bodies(StateScriptNode *ss, FileNode *file, NodeBuilder &builder);

        // ================================================================
        // Bootstrap
        // ================================================================

        // @brief Load lib.st into the interpreter.
        void load_soot_bootstrap();

        // @brief Load lib.sc into the compiler (via the runtime).
        void load_soc_bootstrap();

        // @brief Load the user profile named in cfg.compiler.user_profile.
        //
        // @details "#f" means "no profile". Failures are warnings, not
        //          errors: a broken profile should not prevent the
        //          compiler from starting.
        void load_user_profile();

        // ================================================================
        // Setup
        // ================================================================

        void setup_repl();
        void setup_soot_forms();

        // ================================================================
        // Saving
        // ================================================================

        // @brief Strict-mode check: every method declared in a type
        //        must be implemented by a defmethod in the same file.
        bool check_type_methods_implemented(const FileNode *file_node) const;

        bool save_recursive_impl(const ModuleRef &ref, const std::filesystem::path &out_dir,
                                 std::set<ModuleId> &visited, std::set<ModuleId> &active);

        // ================================================================
        // REPL internals
        // ================================================================

        ReplStatus interpret_and_print(const std::string &script);
        ReplStatus compile_and_report(script::Object forms);
        ReplStatus try_interpret_then_compile(script::Object form);

        // @brief The set of names recognised as REPL commands.
        static const std::set<std::string> &repl_commands();

        // @brief If `form` parses to a REPL command, dispatch it.
        //
        // @return The command's ReplStatus, or std::nullopt if the
        //         input is not a REPL command and should go through
        //         the normal compile pipeline.
        std::optional<ReplStatus> try_handle_repl_command(const script::Object &form);

        // @brief Map a command name to its handler.
        ReplStatus dispatch_repl_command(const std::string &name, const script::Object &form);

        // ================================================================
        // Helpers
        // ================================================================

        void                 color_binary_file(std::unique_ptr<BinaryFile> &binary);
        std::vector<uint8_t> codegen_binary(BinaryFile *binary);

        std::string find_file(const std::string &filename);
        std::string read_file_content(const std::string &filename);

        void print_error(const std::string &context, const std::exception &e);
        void print_warning(const std::string &warning);

        // @brief Throw a CompilerError with a formatted message.
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

        // ================================================================
        // State
        // ================================================================

        // ----- Borrowed configuration -----

        // The single source of truth. Borrowed, not owned: the caller
        // (main, or a test) must keep it alive for the lifetime of the
        // Compiler.
        const common::Config &m_config;

        // ----- Core subsystems -----

        TypeSystem                    &m_ts;
        SootPlatform                   m_platform;
        script::Interpreter            m_soot;
        std::unique_ptr<REPL::Wrapper> m_repl;
        std::unique_ptr<GlobalNode>    m_global_env;
        std::unique_ptr<NoneNode>      m_none;
        std::unique_ptr<Runtime>       m_runtime;

        // ----- Per-session compilation state -----

        ModuleId    m_current_file;
        InputOrigin m_input_origin = InputOrigin::Default;

        // @brief Signatures of native functions, plus a fallback for
        //        REPL and test inputs.
        //
        // @details Script functions in regular files carry their own
        //          signature in FunctionNode::m_signature and are
        //          resolved through FileNode::lookup. This table is
        //          consulted only for native C functions (they have
        //          no FunctionNode) and as a fallback for REPL/test
        //          inputs (they have no import graph connecting one
        //          input to the previous one).
        std::unordered_map<std::string, FunctionInfo>   m_native_functions;
        std::unordered_map<std::string, script::Object> m_constants;

        // Per-instance set of already-loaded bootstrap paths.
        //
        // MUST NOT be static: each Compiler instance has its own
        // interpreter (m_soot), and macro registration is
        // per-interpreter. A static set would silently skip loading
        // lib.sc into the second and later Compiler instances,
        // leaving them without `defun`, `let`, etc., which breaks
        // every unit test that creates a fresh Compiler.
        std::set<std::filesystem::path> m_loaded_soot_libs;
        std::set<std::filesystem::path> m_loaded_soc_libs;

        // @brief All top-level forms entered in the REPL during this
        //        session.
        std::vector<script::Object> m_session_forms;

        // @brief Base name used when saving the REPL session.
        std::string m_session_name = "repl";

        // @brief Monotonic counter for anonymous lambda names.
        u64 m_lambda_counter = 0;

        // @brief Monotonic counter for REPL module names ("<repl-N>").
        //
        // @details Every REPL input is compiled as its own module with
        //          a unique logical name. This is what makes Globals
        //          accumulate symbols across inputs instead of
        //          replacing them: load_module_from_binary unloads any
        //          existing module with the same logical name, so
        //          reusing "<repl>" for every input would erase every
        //          previous input's slots on each new input.
        u64 m_repl_counter = 0;

        // @brief Set by (e) to request REPL exit.
        bool m_want_exit = false;

        // @brief Set by (reload) to request a compiler rebuild.
        bool m_want_reload = false;

        // ----- require tracking -----

        // Logical module ids already loaded in this Compiler instance.
        // Keyed by ModuleId (relative to project_root), not by absolute
        // path: the module identity is stable across project moves.
        std::set<ModuleId> m_loaded_files;

        // Every file compiled in this session, keyed by ModuleId.
        //
        // The FileNode owns the symbol table of that file; `require`
        // needs to keep it alive so that the requirer's FileNode can
        // point at it through m_imports. The FileNode is destroyed
        // only when the Compiler is destroyed.
        std::unordered_map<ModuleId, std::unique_ptr<FileNode>> m_file_nodes;

        // ----- Snapshot / rollback -----

        // Snapshot of GlobalNode taken at the start of compile_internal
        // and rolled back on exception. Guarantees that global_env does
        // not hold a dangling pointer after FileNode rollback.
        GlobalNode::Snapshot m_global_snapshot;
    };

} // namespace sootc