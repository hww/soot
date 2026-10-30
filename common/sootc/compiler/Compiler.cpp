// sootc/compiler/Compiler.cpp
#include "carbon/file/BinaryFileInspector.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/file/SizeAssertions.hpp"
#include "carbon/lib/StringIdManager.hpp"
#include "carbon/vm/VirtualMachine.hpp"

#include "script/Interpreter.hpp"
#include "script/Object.hpp"
#include "script/ParseHelpers.hpp"
#include "script/Reader.hpp"

#include "sootc/compiler/Compiler.hpp"
#include "sootc/compiler/CompilerError.hpp"
#include "sootc/compiler/FileCompiler.hpp"
#include "sootc/compiler/Macro.hpp"
#include "sootc/compiler/NodeBuilder.hpp"

#include "sootc/node/DataDeclarationNode.hpp"
#include "sootc/node/EnumDeclarationNode.hpp"
#include "sootc/node/FileNode.hpp"
#include "sootc/node/HeapDefineNode.hpp"
#include "sootc/node/SequenceNode.hpp"
#include "sootc/node/TypeDeclarationNode.hpp"
#include "sootc/node/StaticDefineNode.hpp"
#include "sootc/node/StateScriptNode.hpp"
#include "sootc/node/NodeInspector.hpp"

#include "sootc/runtime/LocalVmRuntime.hpp"
#include "sootc/runtime/NullRuntime.hpp"
#include "sootc/runtime/RemoteRuntime.hpp"

#include "type_system/TypeSystem.hpp"

#include "fmt/color.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "fmt/ranges.h"

#include "third_party/replxx/include/replxx.hxx"

#include "util/FileUtil.hpp"
#include "util/ProjectConfig.hpp"
#include "util/Log.hpp"
#include <Config.cpp>

namespace sootc {

    // @brief Construct the compiler.
    //
    // @param cfg           The single source of truth for every
    //                      option. Must outlive the Compiler.
    // @param repl_config   Optional REPL shell configuration.
    //                      Not part of cfg: it describes the
    //                      interactive shell, not the compiler.
    // @param user_profile  Profile name, kept for REPL::Wrapper.
    //                      Same value as cfg.compiler.user_profile.
    // @param repl          Optional REPL wrapper.
    Compiler::Compiler(const common::Config &cfg, const std::optional<REPL::Config> repl_config,
             const std::string &user_profile, std::unique_ptr<REPL::Wrapper> repl)
        : m_ts(TypeSystem::instance()), m_platform(cfg.common.platform),
          m_soot(user_profile, false, true, cfg.common.platform), m_repl(std::move(repl)),
          m_config(cfg) {
        (void)repl_config;

        Type::log_level = m_config.common.log_level;


        // If the caller did not set project_root (unit tests, embedded
        // uses), fall back to file_util's PROJECT, or to CWD if that is
        // also empty. The compiler cannot work without a root: every
        // module id is resolved relative to it, and the prelude loader
        // needs a directory to search in.
        if (m_config.common.project_root.empty()) {
            throw std::runtime_error(
                "Compiler: cfg.common.project_root is empty; "
                "call common::set_project_path() and fill cfg before constructing Compiler");
        }

        m_ts.add_builtin_types();
        m_global_env = std::make_unique<GlobalNode>();
        m_none = std::make_unique<NoneNode>();

        // Pick the execution backend. The default is the in-process
        // Carbon VM, which is what sootc has always used. Two
        // alternatives exist:
        //
        //   "null"          -> NullRuntime; used by --compile-only
        //                      and by tests that do not execute code.
        //
        //   "<host>:<port>" -> RemoteRuntime; sends work to an
        //                      external host (carbon.exe) over the
        //                      cross-socket protocol.
        if (m_config.compiler.runtime_url.empty()) {
            // Convert the string search_paths from CompilationOptions
            // into filesystem::path for CarbonConfig. The runtime uses
            // these to find .bin files when a script calls
            // (load-module "name").
            std::vector<std::filesystem::path> runtime_search_paths;
            runtime_search_paths.reserve(m_config.compiler.search_paths.size());
            for (const auto &s : m_config.compiler.search_paths) { runtime_search_paths.emplace_back(s); }
            m_runtime = std::make_unique<LocalVmRuntime>(std::move(runtime_search_paths));
        } else if (m_config.compiler.runtime_url == "null") {
            m_runtime = std::make_unique<NullRuntime>();
        } else {
            m_runtime = std::make_unique<RemoteRuntime>(m_config.compiler.runtime_url);
        }
        lg::info("Runtime: {}", m_runtime->name());

        // ---------------------------------------------------------------------
        // SIDBase loading
        // ---------------------------------------------------------------------
        //
        // The SIDBase is a binary map from SID64 hashes back to their
        // original strings. It is the only way to recover a human-
        // readable name from a SID that appears in a compiled binary
        // (for the disassembler, the inspector, error messages, ...).
        //
        // We load it here — BEFORE load_soot_prelude and lib.sc — so
        // that every string registered during prelude loading (lib.st
        // and lib.sc contain many symbol names) is available for
        // reverse lookup from the very first byte compiled in this
        // process.
        //
        // If sidbase_path is empty, nothing is loaded: the process
        // starts with an empty SID registry. That is exactly what the
        // unit tests rely on, because they never want to touch the
        // user's sidbase file.
        if (!m_config.common.sidbase.string().empty()) {
            std::error_code ec;
            const bool      exists = std::filesystem::exists(m_config.common.sidbase.string(), ec);

            if (ec) {
                lg::warn("SIDBase: cannot stat '{}': {} — starting empty", m_config.common.sidbase.string(),
                         ec.message());
            } else if (!exists) {
                lg::info("SIDBase: '{}' not found — will be created on exit",
                         m_config.common.sidbase.string());
            } else {
                const bool loaded = carbon::StringIdManager::instance().load_dconstruct_sidbase(
                    m_config.common.sidbase.string());

                if (loaded) {
                    lg::info("SIDBase: loaded {} entries from '{}'",
                             carbon::StringIdManager::instance().size(), m_config.common.sidbase.string());
                } else {
                    lg::warn("SIDBase: failed to load '{}' — starting empty",
                             m_config.common.sidbase.string());
                }
            }
        } else {
            lg::info("SIDBase: no path configured — using in-memory registry only");
        }

        // 1. SOOT bootstrap -> interpreter (lib.st).
        load_soot_bootstrap();

        // 2. SOOTC bootstrap -> compiler (lib.sc).
        load_soc_bootstrap();

        if (m_config.compiler.mode == "compile") { setup_repl(); }

        if (user_profile != "#f") { load_user_profile(); }

        // REPL wiring (history, welcome, callbacks).
        if (m_repl) {
            m_repl->load_history();

            m_repl->examples = {"(define ",      "(defun ",       "(defmacro ", "(if ",
                                "(when ",        "(unless ",      "(cond ",     "(let ",
                                "(lambda ",      "(set! ",        "(quote ",    "(load \"",
                                "(compile \"",   "(deftype ",     "(defenum ",  "(sc-run ",
                                "(sc-eval ",     "(inspect-env)", "(e)",        "(reload)",
                                "(help)",        "(clear)",       "(sizes)",    "(list-symbols)",
                                "(save-session ", "(load-module"};

            m_repl->regex_colors = {
                {";[^\n]*", replxx::Replxx::Color::BRIGHTCYAN},
                {"\"[^\"]*\"", replxx::Replxx::Color::GREEN},
                {"\\b[0-9]+\\b", replxx::Replxx::Color::YELLOW},
                {"\\b(defun|defmacro|if|when|unless|cond|let|lambda|set!|define|quote|deftype|"
                 "defenum|defmethod|defconstant|defenum|sc-run|sc-eval|inspect-env|e|reload|"
                 "help|clear|sizes|list-symbols|save-session)\\b",
                 replxx::Replxx::Color::BRIGHTMAGENTA},
                {"\\b#t\\b|\\b#f\\b", replxx::Replxx::Color::BRIGHTMAGENTA},
                {"[\\(\\)]", replxx::Replxx::Color::BRIGHTBLUE},
            };

            m_repl->init_settings();

            using namespace std::placeholders;
            m_repl->get_repl().set_completion_callback(
                std::bind(&Compiler::find_symbols_or_object_file_by_prefix, this, _1, _2,
                          std::cref(m_repl->examples)));

            m_repl->get_repl().set_hint_callback(std::bind(
                &Compiler::find_hints_by_prefix, this, _1, _2, _3, std::cref(m_repl->examples)));

            m_repl->get_repl().set_highlighter_callback(
                std::bind(&Compiler::repl_coloring, this, _1, _2));
        }

        // C++ forms that work with the compiler's internal state.
        setup_soot_forms();
    }

    Compiler::~Compiler() {
        // SIDBase saving is the caller's responsibility, not ours.
        //
        // We deliberately do NOT save the sidbase here. Reasons:
        //
        //   1. The caller (main.cpp, or a test) knows best when the
        //      compilation "session" is over. For a CLI run with several
        //      input files, that is after the last file; for a REPL, that
        //      is on exit; for a unit test, that is never.
        //
        //   2. A destructor is the wrong place for a side-effecting
        //      operation that can fail. If saving throws (disk full,
        //      permission denied), a destructor must not propagate the
        //      exception, and swallowing it silently loses data.
        //
        //   3. If the process crashes, the destructor does not run at all,
        //      so relying on it gives a false sense of safety.
        //
        // The caller should invoke StringIdManager::instance().save_dconstruct_sidbase()
        // explicitly at the point where the session ends. See main.cpp
        // for the CLI and REPL paths.
    }

    // ===============================================================
    // Compiler-side SOOT forms
    // ===============================================================
    //
    // Only `get-enum-vals` is registered here. It is the ONLY custom
    // form that is part of the LANGUAGE and must therefore be visible
    // to the compiler's pipeline: lib.sc uses it inside macro expansion
    // to resolve enum values.
    //
    // All REPL commands — help, clear, e, reload, sizes, inspect-env,
    // inspect-env-soots, has-macro, list-symbols, save-session, sc-run,
    // sc-eval — are NOT registered here. They are handled by
    // handle_repl_string before the pipeline runs; see repl_commands()
    // and dispatch_repl_command().
    void Compiler::setup_soot_forms() {
        script::ArgumentSpec spec(true, false);

        m_soot.add_custom_form(
            "get-enum-vals",
            [this](const script::Object &form, script::Arguments &args,
                   const std::shared_ptr<script::EnvironmentObject> &env) -> script::Object {
                return builtin_get_enum_vals(form, args, env);
            },
            &spec);
    }

    // Built-in get-enum-vals. Returns a list of (symbol . value) pairs
    // for the enum whose name is given as the first argument.
    script::Object
    Compiler::builtin_get_enum_vals(const script::Object &form, script::Arguments &args,
                                    const std::shared_ptr<script::EnvironmentObject> &env) {
        (void)form;
        (void)env;

        if (args.unnamed.empty()) {
            throw std::runtime_error("get-enum-vals: expected enum name as argument");
        }

        const auto &enum_obj = args.unnamed[0];
        if (!enum_obj.is_symbol()) {
            throw std::runtime_error("get-enum-vals: expected symbol as enum name");
        }

        const auto &enum_name = enum_obj.as_symbol().name_ptr;
        auto        enum_type = m_ts.try_enum_lookup(enum_name);

        if (!enum_type) {
            throw std::runtime_error(fmt::format("get-enum-vals: unknown enum '{}'", enum_name));
        }

        std::vector<std::pair<std::string, int64_t>> sorted_values;
        for (auto &val : enum_type->entries()) {
            sorted_values.emplace_back(val.first, enum_type->is_bitfield()
                                                      ? static_cast<int64_t>(1) << val.second
                                                      : val.second);
        }

        std::sort(sorted_values.begin(), sorted_values.end(),
                  [](const auto &a, const auto &b) { return a.second < b.second; });

        std::vector<script::Object> enum_vals;
        for (auto &thing : sorted_values) {
            enum_vals.push_back(script::Object::make_pair(
                script::Object::make_symbol(&m_soot.symbol_table(), thing.first),
                script::Object::make_integer(thing.second)));
        }

        return script::build_list(enum_vals);
    }

    // Parse arguments into a script::Arguments format. Throws a compiler error
    // if parsing fails.
    script::Arguments Compiler::get_va(const script::Object &form, const script::Object &rest) {
        script::Arguments args;
        std::string       err;
        if (!script::get_va(rest, &err, &args)) { throw_compiler_error(form, "{}", err); }
        return args;
    }

    // Same as get_va, but does not handle named arguments.
    script::Arguments Compiler::get_va_no_named(const script::Object &form,
                                                const script::Object &rest) {
        (void)form;
        script::Arguments args;
        script::get_va_no_named(rest, &args);
        return args;
    }

    // Validate the parsed arguments (unnamed/named) and throw a compiler
    // error if the check fails.
    void Compiler::va_check(
        const script::Object &form, const script::Arguments &args,
        const std::vector<std::optional<script::ObjectType>> &unnamed,
        const std::unordered_map<std::string, std::pair<bool, std::optional<script::ObjectType>>>
            &named) {
        std::string err;
        if (!script::va_check(args, unnamed, named, &err)) {
            throw_compiler_error(form, "{}", err);
        }
    }

    // Walk a proper list and invoke f on each element. Throws a compiler
    // error if the list is not proper.
    void Compiler::for_each_in_list(const script::Object                              &list,
                                    const std::function<void(const script::Object &)> &f) {
        const script::Object *iter = &list;
        while (iter->is_pair()) {
            auto lap = iter->as_pair();
            f(lap->car);
            iter = &lap->cdr;
        }

        if (!iter->is_null()) { throw_compiler_error(list, "Invalid list: {}", list.print()); }
    }
    // ===============================================================
    // Module resolution
    // ===============================================================

    namespace {

        /// Canonicalise `path` to a ModuleId (relative to `root`).
        /// Rejects paths that escape `root`.
        ModuleId make_module_id(const std::filesystem::path &path,
                                const std::filesystem::path &root) {
            namespace fs = std::filesystem;
            std::error_code ec;
            fs::path        canonical = fs::weakly_canonical(path, ec);
            if (ec) canonical = fs::absolute(path);

            fs::path root_canonical = fs::weakly_canonical(root, ec);
            if (ec) root_canonical = fs::absolute(root);

            fs::path rel = canonical.lexically_relative(root_canonical);
            if (rel.empty()) {
                throw std::runtime_error(fmt::format("module '{}' is outside project root '{}'",
                                                     path.string(), root.string()));
            }
            for (const auto &part : rel) {
                if (part == "..") {
                    throw std::runtime_error(fmt::format("module '{}' escapes project root '{}'",
                                                         path.string(), root.string()));
                }
            }
            return rel;
        }

    } // namespace

    // ========== Compilation ==========

    std::expected<std::unique_ptr<BinaryFile>, std::string>
    Compiler::compile_file(script::Object &forms, const std::string &filename) {
        m_current_file = ModuleId(filename);
        m_input_origin = InputOrigin::Default;
        auto result = compile_internal(forms, filename, /*is_repl=*/false);

        if (result && m_config.compiler.dump_ir) {
            lg::info("Compilation successful for: {}", filename);
            BinaryFileInspector inspector(result->get());
            inspector.inspect();
        }
        return result;
    }

    std::expected<std::unique_ptr<BinaryFile>, std::string>
    Compiler::compile_file(const std::filesystem::path &path) {
        // Canonicalize the key: the same file reached through different
        // spellings (relative, absolute, symlinks) must resolve to the
        // same ModuleId.
        const ModuleId    id = make_module_id(path, m_config.common.project_root);
        const std::string content = read_file_content((m_config.common.project_root / id).string());
        auto              forms = m_soot.get_reader().read_from_string(content, false, id.string());
        if (forms.is_null()) {
            return std::unexpected("Failed to read or parse file: " + id.string());
        }
        return compile_file(forms, id.string());
    }

    std::expected<std::unique_ptr<BinaryFile>, std::string>
    Compiler::compile_file_repl(script::Object &forms, const std::string &filename) {
        m_current_file = ModuleId(filename);
        m_input_origin = InputOrigin::Repl;
        auto result = compile_internal(forms, filename, /*is_repl=*/true);

        if (result && m_config.compiler.dump_ir) {
            lg::info("Compilation successful for: {}", filename);
            BinaryFileInspector inspector(result->get());
            inspector.inspect();
        }
        return result;
    }

    std::optional<ModuleRef> Compiler::resolve_require(const std::string &path,
                                                       const ModuleId    &from_id) const {
        namespace fs = std::filesystem;

        std::vector<fs::path> search_paths;
        for (const auto &s : m_config.compiler.search_paths) search_paths.emplace_back(s);

        // Base directory for explicit ./ and ../ paths.
        //
        //   Default — the file's own directory.
        //   Test    — project_root (no real file on disk).
        //   Repl    — project_root (no real file on disk).
        //
        // This is decided by the compiler's own state, not by
        // inspecting the ModuleId string: a synthetic name is an
        // implementation detail, not an input to path resolution.
        fs::path base_dir;
        if (m_input_origin == InputOrigin::Default) {
            base_dir = from_id.parent_path();
        } else {
            base_dir = m_config.common.project_root;
        }

        fs::path found = common::find_project_file(path, base_dir, search_paths);
        if (found.empty()) return std::nullopt;

        auto id_opt = [&]() -> std::optional<ModuleId> {
            try {
                return make_module_id(found, m_config.common.project_root);
            } catch (...) { return std::nullopt; }
        }();
        if (!id_opt) return std::nullopt;

        std::error_code ec;
        fs::path        canon = fs::weakly_canonical(found, ec);
        if (ec) canon = fs::absolute(found);
        return ModuleRef{*id_opt, canon};
    }

    std::optional<ModuleRef> Compiler::resolve_module(const std::string &path) const {
        namespace fs = std::filesystem;
        const fs::path root = m_config.common.project_root;

        // Absolute: convert to ModuleId.
        if (fs::path(path).is_absolute()) {
            if (!fs::exists(path)) return std::nullopt;
            auto id_opt = [&]() -> std::optional<ModuleId> {
                try {
                    return make_module_id(path, root);
                } catch (...) { return std::nullopt; }
            }();
            if (!id_opt) return std::nullopt;
            std::error_code ec;
            fs::path        canon = fs::weakly_canonical(path, ec);
            if (ec) canon = fs::absolute(path);
            return ModuleRef{*id_opt, canon};
        }

        // Relative to project root. Try with and without .sc.
        fs::path candidate = path;
        if (!candidate.has_extension()) {
            fs::path with_sc = root / (path + ".sc");
            if (fs::exists(with_sc)) {
                auto id_opt = [&]() -> std::optional<ModuleId> {
                    try {
                        return make_module_id(with_sc, root);
                    } catch (...) { return std::nullopt; }
                }();
                if (!id_opt) return std::nullopt;
                std::error_code ec;
                fs::path        canon = fs::weakly_canonical(with_sc, ec);
                if (ec) canon = fs::absolute(with_sc);
                return ModuleRef{*id_opt, canon};
            }
        }
        fs::path resolved = root / candidate;
        if (fs::exists(resolved)) {
            auto id_opt = [&]() -> std::optional<ModuleId> {
                try {
                    return make_module_id(resolved, root);
                } catch (...) { return std::nullopt; }
            }();
            if (!id_opt) return std::nullopt;
            std::error_code ec;
            fs::path        canon = fs::weakly_canonical(resolved, ec);
            if (ec) canon = fs::absolute(resolved);
            return ModuleRef{*id_opt, canon};
        }

        return std::nullopt;
    }

       // (require "path/to/file.sc")
    //
    // Implements require as a language form. Called by NodeBuilder when
    // it sees `require` at the head of a form during compilation.
    //
    // A require path is resolved against ONE root: the project root.
    // There are two forms:
    //
    //   1. "./foo.sc" or "../foo.sc" — relative to the directory of
    //      the file that contains the require. Use this when the
    //      dependency lives next to its dependent and should move
    //      with it.
    //
    //   2. "lib/utils.sc" — relative to the project root. Use this
    //      for stable, project-wide references. This is the form
    //      that ends up in the .bin's "requires" entry.
    //
    // Absolute paths are rejected: a module must not depend on an
    // arbitrary file outside the project. That would break
    // reproducibility and make the .bin non-portable.
    //
    // The resolved path is canonicalised and converted to a ModuleId
    // (a path relative to the project root). The ModuleId is what
    // m_loaded_files and m_file_nodes are keyed by, so two different
    // spellings of the same file resolve to the same module, and the
    // same module resolves to the same key on any machine.
    //
    // If the module was already loaded in this Compiler instance,
    // the call is a no-op. Otherwise the file is compiled and the
    // resulting module is registered in the active runtime.
    //
    // Cyclic requires are not supported: a file that requires itself,
    // directly or transitively, is silently skipped on the second
    // visit. The entry is added to m_loaded_files BEFORE the recursive
    // compile, so the recursion terminates. This is a deliberate
    // trade-off: it matches C/C++ #include guards, where a cycle is
    // a no-op rather than an error. A future version may report
    // cycles explicitly (see save_module_recursive, which does).
    bool Compiler::require_file(const script::Object &form, const std::string &path) {
        (void)form;

        // ------------------------------------------------------------------
        // 1. Resolve the path to a ModuleRef.
        // ------------------------------------------------------------------
        //
        // resolve_require() applies the two rules above and returns
        // a ModuleRef { ModuleId, ResolvedPath }. Absolute paths and
        // paths outside the project root are rejected.
        //
        // No CWD search, no search_paths search: those were removed
        // deliberately. A module's identity must be stable across
        // machines and working directories.
        auto ref_opt = resolve_require(path, m_current_file);
        if (!ref_opt) {
            throw CompilerError("Compiler::require_file")
                .expected("a file inside the project root, "
                          "or an explicit ./sibling or ../sibling")
                .got(fmt::format("'{}'", path))
                .note(fmt::format("PROJECT: {}", m_config.common.project_root.string()));
        }
        const ModuleRef &ref = *ref_opt;

        // ------------------------------------------------------------------
        // 2. Skip if already loaded.
        // ------------------------------------------------------------------
        //
        // m_loaded_files is keyed by ModuleId, not by absolute path.
        // The same module reached through different spellings (or on
        // different machines) produces the same ModuleId, so the
        // check is stable.
        //
        // Even when the module is already loaded, the CURRENT requirer
        // still needs an import edge to it: FileNode::lookup walks
        // m_imports recursively, and without this edge the requirer
        // cannot see the module's symbols. This matters when the same
        // module is required from two places, e.g. once from the top
        // level of a REPL session and once from a source file inside
        // the same session.
        if (m_loaded_files.count(ref.id)) {
            lg::debug("require: '{}' already loaded — adding import edge", ref.id.string());

            FileNode *from_file = find_file_node(m_current_file);
            FileNode *dep_file = find_file_node(ref.id);
            if (from_file && dep_file) {
                from_file->add_import(dep_file);
                lg::debug("require: '{}' now imports '{}' (cached)", from_file->name(),
                          ref.id.string());
            }
            return true;
        }

        // Mark as loaded BEFORE compiling. This is what makes cyclic
        // requires terminate: if the file we are about to compile
        // requires itself (directly or transitively), the inner call
        // sees the key in m_loaded_files and returns immediately.
        m_loaded_files.insert(ref.id);

        // ------------------------------------------------------------------
        // 3. Compile and load.
        // ------------------------------------------------------------------
        lg::info("require: loading '{}'", ref.id.string());

        // Remember the requirer's FileNode BEFORE compile_file, because
        // compile_file overwrites m_current_file with the required
        // module's id. If we looked it up after compile_file, we would
        // see the required module, not its requirer, and the
        // add_import call below would be a self-import (which
        // FileNode::add_import ignores).
        FileNode *requirer_file = find_file_node(m_current_file);

        auto result = compile_file(ref.resolved);

        if (!result) {
            // Compilation failed. Remove the marker so that a later
            // require of the same file can be retried after the user
            // fixes the problem.
            m_loaded_files.erase(ref.id);
            throw CompilerError("Compiler::require_file")
                .expected("a compilable file")
                .got(fmt::format("'{}'", ref.id.string()))
                .caused_by(result.error());
        }

        // `*result` is the unique_ptr<BinaryFile>. It is empty when the
        // file contained only compile-time declarations (deftype /
        // defenum / defconstant). That is a normal outcome: the types
        // are already registered in TypeSystem, so there is nothing to
        // load into the runtime.
        if (!*result) {
            lg::debug("require: '{}' produced no binary (compile-time only)", ref.id.string());
            return true;
        }

        bool loaded = m_runtime->load_module(std::move(**result));
        if (!loaded) {
            m_loaded_files.erase(ref.id);
            throw CompilerError("Compiler::require_file")
                .expected("the runtime to accept the module")
                .got(fmt::format("runtime '{}' rejected '{}'", m_runtime->name(), ref.id.string()));
        }

        // Record the dependency edge: the file that contains the
        // (require ...) form now points at the FileNode of the
        // required module. This is what FileNode::lookup walks when
        // resolving names. Without it, a function defined in the
        // required file is not visible to the requirer, even though
        // it is registered in the flat m_native_functions table.
        //
        // m_current_file identifies the requirer:
        //   * for a regular source file, it is the file's ModuleId;
        //   * for a REPL input, it is the synthetic "<repl-N>" name,
        //     which has a FileNode in m_file_nodes.
        {
            FileNode *from_file = requirer_file;
            FileNode *dep_file = find_file_node(ref.id);
            if (from_file && dep_file) {
                from_file->add_import(dep_file);
                lg::debug("require: '{}' now imports '{}'", from_file->name(), ref.id.string());
            }
        }

        lg::info("require: loaded '{}'", ref.id.string());
        return true;
    }


    // The core compilation driver: build the node tree, generate a binary,
    // and wrap it in a BinaryFile. Returns nullptr (wrapped in a unique_ptr)
    // if the input produced no binary output (compile-time-only declarations).
    // The core compilation driver: build the node tree, generate a binary,
    // and wrap it in a BinaryFile. Returns nullptr (wrapped in a unique_ptr)
    // if the input produced no binary output (compile-time-only declarations).
    std::expected<std::unique_ptr<BinaryFile>, std::string>
    Compiler::compile_internal(script::Object &forms, const std::string &filename, bool is_repl) {
        std::set<FileNode *> owners_to_rebuild;
        // ---- Effective name ----
        //
        // For REPL inputs, generate a unique logical name. The name
        // is used as the FileNode key, the BinaryFile path, and (via
        // LocalVmRuntime) the module's logical name in the runtime.
        //
        // Uniqueness is essential: load_module_from_binary unloads any
        // existing module whose logical name matches. If every REPL
        // input were called "<repl>", each new input would unload the
        // previous one, dropping its symbols from Globals and freeing
        // its bytes. The slot of a heap-defined variable would then
        // dangle, and a later (-> v x) would read zero.
        std::string effective_filename = filename;
        if (is_repl) { effective_filename = fmt::format("<repl-{}>", m_repl_counter++); }
        
        // For REPL inputs, the effective name is "<repl-N>". Make it
        // visible to resolve_require via m_current_file, so that
        // (require "./sibling.sc") from the REPL knows there is no real
        // file to anchor to and falls back to PROJECT.
        if (is_repl) { m_current_file = ModuleId(effective_filename); }

        FileNode          *file_node = nullptr;
        bool               owns_file_node = false;
        FileNode::Snapshot snapshot;

        // Snapshot global_env before compiling. build_define may bind a
        // name in global_env() for a node that is then destroyed along
        // with the FileNode on rollback. Without this, global_env holds
        // a dangling pointer and the next input crashes in build_variable.
        m_global_snapshot = m_global_env->take_snapshot();

        try {
            NodeBuilder builder(m_ts, this);

            // ---- Reuse or create the FileNode for this name ----
            //
            // For a REPL input, effective_filename is unique, so this
            // never finds an existing entry: every REPL input gets a
            // fresh FileNode. For a normal file (compiled once per
            // process, or recompiled after (require) skipped it), the
            // same FileNode is reused.
            auto it = m_file_nodes.find(effective_filename);
            if (it != m_file_nodes.end()) {
                file_node = it->second.get();
            } else {
                auto owned = std::make_unique<FileNode>(effective_filename);
                file_node = owned.get();
                m_file_nodes[effective_filename] = std::move(owned);
                owns_file_node = true;
            }

            snapshot = file_node->take_snapshot();

            // ---- Top-level function (lazy) ----
            //
            // top_level is created ONLY when the first top-level expression
            // appears. A file that contains only declarations (deftype /
            // defenum / defconstant / defun) has no top-level expressions
            // and therefore no top-level function: nothing would run, and
            // emitting an empty SsLambda("top-level") would add a bogus
            // entry to the binary.
            //
            // For a reused FileNode (non-REPL), top_level may already exist
            // from a previous compile; in that case we keep it and replace
            // its body for THIS input.
            FunctionNode *top_level = nullptr;
            SequenceNode *top_level_body = nullptr;

            auto ensure_top_level = [&]() -> SequenceNode * {
                if (!top_level) { top_level = file_node->top_level(); }
                if (!top_level) {
                    auto tl = std::make_unique<FunctionNode>("top-level");
                    top_level = tl.get();
                    file_node->add_child(std::move(tl));
                    file_node->set_top_level(top_level);
                }
                if (!top_level_body) {
                    auto body = std::make_unique<SequenceNode>();
                    top_level_body = body.get();
                    top_level->set_body(std::move(body));
                }
                return top_level_body;
            };

            // ---- Process each top-level form in this input ----
            auto current = forms;
            while (current.is_pair()) {
                auto node = builder.build(current.as_pair()->car, file_node);
                if (node) {
                    if (auto *sd = dynamic_cast<StaticDefineNode *>(node.get())) {
                        file_node->add_child(sd->take_decl());
                        auto *body = ensure_top_level();
                        for (auto &store : sd->take_stores()) { body->add(std::move(store)); }
                    } else if (auto *hd = dynamic_cast<HeapDefineNode *>(node.get())) {
                        file_node->add_child(hd->take_decl());
                        ensure_top_level()->add(hd->take_store());
                    } else if (auto *fn = dynamic_cast<FunctionNode *>(node.get())) {
                        // A FunctionNode whose name is "<type>-<method>" is a method
                        // implementation. If the type was declared by a deftype in a
                        // *previous* REPL input, the type's SsType lives in a different
                        // FileNode. Attach the FunctionNode to that FileNode, not to
                        // the current one, so that when the owner's make_binary runs
                        // it will pick up the method's ScriptLambda and set
                        // SsType::m_pMethods[id].m_pLambda.
                        //
                        // The name is "<type>-<method>", and BOTH parts may contain '-'.
                        // "my-basic-len" is type "my-basic", method "len", not type "my".
                        // We therefore iterate over every known type and check whether
                        // "<type>-" is a prefix of the function name. The longest match
                        // wins, so a type whose name is a prefix of another type's name
                        // ("vec" vs "vector") still resolves correctly.
                        FileNode          *owner = nullptr;
                        size_t             best_prefix_len = 0;
                        const std::string &fn_name = fn->name();

                        for (auto &[oname, onode] : m_file_nodes) {
                            (void)oname;
                            for (auto &child : onode->children()) {
                                if (auto *td = dynamic_cast<TypeDeclarationNode *>(child.get())) {
                                    const std::string prefix = td->type().base_type() + "-";
                                    if (fn_name.rfind(prefix, 0) == 0 &&
                                        prefix.size() > best_prefix_len) {
                                        best_prefix_len = prefix.size();
                                        owner = onode.get();
                                    }
                                }
                            }
                        }

                        if (owner && owner != file_node) {
                            // Replace any existing method implementation in the owner.
                            // Without this, repeated `defmethod len my-basic` in separate
                            // REPL inputs would accumulate duplicate FunctionNodes in
                            // <repl-0>, growing the binary on each call. Removing the
                            // stale child first guarantees exactly one FunctionNode per
                            // method name in the owner's m_children.
                            owner->remove_named(fn_name);

                            FunctionNode *raw = fn;
                            node.release();
                            owner->add_child(std::unique_ptr<Node>(raw));
                            owner->bind(fn_name, raw);
                            owners_to_rebuild.insert(owner);

                            lg::info("compile_internal: attached '{}' to owner '{}'", fn_name,
                                     owner->name());
                        } else {
                            file_node->add_child(std::move(node));
                        }
                    } else if (auto *td = dynamic_cast<TypeDeclarationNode *>(node.get())) {
                        // A repeated `deftype` for a type that is already
                        // declared in another FileNode must NOT create a
                        // second TypeDeclarationNode. ...
                        //
                        // This applies only to REPL inputs. A non-REPL
                        // file is its own unit: a `deftype` in a
                        // different file is a separate declaration,
                        // even if TypeSystem already knows the type.
                        // Dropping it would drop the SsType entry from
                        // the new binary.
                        bool              skip = false;
                        const std::string type_name = td->type().base_type();

                        if (is_repl) {
                            for (auto &[oname, onode] : m_file_nodes) {
                                (void)oname;
                                if (onode.get() == file_node) continue;
                                for (auto &child : onode->children()) {
                                    if (auto *other_td =
                                            dynamic_cast<TypeDeclarationNode *>(child.get())) {
                                        if (other_td->type().base_type() == type_name) {
                                            lg::info("compile_internal: '{}' already declared "
                                                     "in '{}', skipping",
                                                     type_name, onode->name());
                                            skip = true;
                                            break;
                                        }
                                    }
                                }
                                if (skip) break;
                            }
                        }

                        if (!skip) { file_node->add_child(std::move(node)); }
                    } else if (dynamic_cast<EnumDeclarationNode *>(node.get()) != nullptr ||
                               dynamic_cast<StateScriptNode *>(node.get()) != nullptr) {
                        file_node->add_child(std::move(node));
                    } else if (dynamic_cast<DataDeclarationNode *>(node.get()) != nullptr) {
                        file_node->add_child(std::move(node));
                    } else if (dynamic_cast<NewNode *>(node.get()) != nullptr) {
                        throw CompilerError("Compiler::compile_internal")
                            .where(fmt::format("file '{}'", effective_filename))
                            .expected("top-level 'new' to be wrapped in 'define'")
                            .got("bare (new ...)");
                    } else if (auto *expr = dynamic_cast<ExpressionNode *>(node.get())) {
                        if (auto *seq = dynamic_cast<SequenceNode *>(expr);
                            seq && seq->size() == 0) {
                            // Empty sequence at top level: no code, drop it.
                            // `node` is a unique_ptr; going out of scope frees it.
                        } else {
                            node.release();
                            ensure_top_level()->add(std::unique_ptr<ExpressionNode>(expr));
                        }
                    } else {
                        throw CompilerError("Compiler::compile_internal")
                            .where(fmt::format("file '{}'", effective_filename))
                            .expected("top-level form to be FunctionNode, TypeDeclarationNode, "
                                      "EnumDeclarationNode, or ExpressionNode")
                            .got(node->get_node_type_string());
                    }
                }
                if (m_config.compiler.dump_ast) inspect_ast(node.get());
                current = current.as_pair()->cdr;
            }

            // ---- Compile state-script handler bodies ----
            //
            // Each handler body and each track step is compiled into its
            // own FunctionNode, added to the file, and later laid out by
            // FileNode::make_binary. build_state_script reads the names
            // back from entry_offsets to fill SsOnBlock::m_pScriptLambda
            // and SsLambda::m_pScriptLambda.
            //
            // IMPORTANT: compile_state_script_bodies calls file->add_child(),
            // which may reallocate m_children and invalidate every iterator
            // and reference into it. We therefore:
            //   1. take a snapshot of the StateScriptNode* pointers first;
            //   2. then iterate the snapshot, calling add_child on the file.
            {
                std::vector<StateScriptNode *> state_scripts;
                for (auto &child : file_node->children()) {
                    if (auto *ss = dynamic_cast<StateScriptNode *>(child.get())) {
                        state_scripts.push_back(ss);
                    }
                }
                for (auto *ss : state_scripts) {
                    compile_state_script_bodies(ss, file_node, builder);
                }
            }

            // ---- Generate the binary ----
            FileNode   *stored = file_node;
            DataSegment strings_table;
            auto        element = stored->generate(strings_table);

            // ---- Rebuild only the owners that received a new method ----
            //
            // A REPL input that is only a defmethod produces no binary of
            // its own (element.m_rawData is empty), but it has already
            // attached a FunctionNode to the owner FileNode. The owner's
            // SsType must be rebuilt so the new m_pLambda reaches Globals.
            //
            // Rebuild ONLY the owners recorded in owners_to_rebuild, not every
            // FileNode that happens to have a TypeDeclarationNode. Rebuilding
            // every type-owner on every REPL input re-loads lib.sc and
            // overwrites Globals["top-level"], so the module that was just
            // compiled (e.g. <repl-3>) is never the one that runs.
            for (FileNode *owner : owners_to_rebuild) {
                if (!owner || owner == stored) continue;

                DataSegment owner_strings;
                auto        owner_el = owner->generate(owner_strings);
                if (owner_el.m_rawData.empty()) continue;

                auto owner_bytes = make_aligned_buffer(owner_el.m_rawData.size());
                std::memcpy(owner_bytes.get(), owner_el.m_rawData.data(),
                            owner_el.m_rawData.size());

                auto owner_bin_res = BinaryFile::from_buffer(owner->name(), std::move(owner_bytes),
                                                             owner_el.m_rawData.size());
                if (!owner_bin_res) {
                    lg::warn("compile_internal: cannot rebuild owner '{}': {}", owner->name(),
                             owner_bin_res.error());
                    continue;
                }

                auto owner_bin = std::make_unique<BinaryFile>(std::move(owner_bin_res.value()));
                for (const auto &ds : owner->data_structs()) {
                    owner_bin->m_dataStructs.push_back(ds);
                }

                if (!m_runtime->load_module(std::move(*owner_bin))) {
                    lg::warn("compile_internal: runtime rejected rebuilt owner '{}'",
                             owner->name());
                } else {
                    lg::debug("compile_internal: rebuilt owner '{}'", owner->name());
                }
            }

            if (element.m_rawData.empty()) {
                lg::info("No binary output (only type/enum declarations)");
                return std::unique_ptr<BinaryFile>(nullptr);
            }

            if (m_config.compiler.dump_ir) { element.dump(); }

            auto bytes = make_aligned_buffer(element.m_rawData.size());
            std::memcpy(bytes.get(), element.m_rawData.data(), element.m_rawData.size());

            auto binary_result = BinaryFile::from_buffer(effective_filename, std::move(bytes),
                                                         element.m_rawData.size());
            if (!binary_result) {
                return std::unexpected("Failed to create binary from buffer: " +
                                       binary_result.error());
            }

            auto binary = std::make_unique<BinaryFile>(std::move(binary_result.value()));

            for (const auto &ds : stored->data_structs()) { binary->m_dataStructs.push_back(ds); }

            if (m_config.compiler.dump_asm) { color_binary_file(binary); }

            m_global_snapshot = GlobalNode::Snapshot{};

            return binary;

        } catch (const script::EvalException &e) {
            if (file_node && snapshot.valid) {
                file_node->rollback(std::move(snapshot));
                if (owns_file_node) { m_file_nodes.erase(effective_filename); }
            }
            m_global_env->rollback(std::move(m_global_snapshot));
            return std::unexpected(std::string("SOOT error: ") + e.what());

        } catch (const CompilerError &e) {
            if (m_config.compiler.dump_ast && file_node) { inspect_ast(file_node); }
            if (file_node && snapshot.valid) {
                file_node->rollback(std::move(snapshot));
                if (owns_file_node) { m_file_nodes.erase(effective_filename); }
            }
            m_global_env->rollback(std::move(m_global_snapshot));
            return std::unexpected(std::string(e.what()));

        } catch (const std::exception &e) {
            if (file_node && snapshot.valid) {
                file_node->rollback(std::move(snapshot));
                if (owns_file_node) { m_file_nodes.erase(effective_filename); }
            }
            m_global_env->rollback(std::move(m_global_snapshot));
            render_internal_error(e);
            return std::unexpected(std::string("Internal compiler error: ") + e.what());
        }
    }

    // Render a C++ exception that escaped NodeBuilder/Compiler internals.
    // This is a bug in the compiler, not in user code.
    void Compiler::render_internal_error(const std::exception &e) {
        fmt::print(fg(fmt::color::dim_gray), "Exception: {}\n", e.what());
        fmt::print(fg(fmt::color::dim_gray), "Type:      {}\n", typeid(e).name());
    }

    // ===============================================================
    // REPL dispatch
    // ===============================================================

    // Dispatch a REPL line.
    //
    // There is no separate `:command` namespace. Every input is an
    // ordinary S-expression. If the form is a REPL command (head is a
    // symbol in repl_commands()), it is dispatched to the corresponding
    // cmd_* handler before the pipeline runs. Anything else is code and
    // goes through compile_and_report / interpret_and_print.
    ReplStatus Compiler::handle_repl_string(const std::string &input) {
        const auto first = input.find_first_not_of(" \t");
        if (first == std::string::npos) return ReplStatus::OK;

        std::string trimmed = input.substr(first);
        m_want_exit = false;
        m_want_reload = false;

        auto forms = m_soot.get_reader().read_from_string(trimmed, false, "<repl>");
        if (forms.is_null()) return ReplStatus::ERR;

        auto status = compile_and_report(forms);
        if (status == ReplStatus::WANT_EXIT) return ReplStatus::WANT_EXIT;
        if (status == ReplStatus::WANT_RELOAD) return ReplStatus::WANT_RELOAD;
        return status;
    }
    // ===============================================================
    // REPL loop helpers
    // ===============================================================

    void Compiler::save_repl_history() { m_repl->save_history(); }

    void Compiler::print_to_repl(const std::string &str) { m_repl->print_to_repl(str); }

    // The prompt depends on the current mode.
    std::string Compiler::get_prompt() {
        std::string prompt = "";

        if (m_config.compiler.mode == "compile") {
            prompt = fmt::format(fmt::emphasis::bold | fg(fmt::color::cyan), "sc > ");
        } else if (m_config.compiler.mode == "interpret") {
            prompt = fmt::format(fmt::emphasis::bold | fg(fmt::color::cyan), "si > ");
        } else {
            prompt = fmt::format(fmt::emphasis::bold | fg(fmt::color::cyan), "sci> ");
        }
        return "\033[0m" + prompt;
    }

    // Read a complete REPL expression. Supports multi-line input by
    // continuing until the reader says the expression is complete.
    std::string Compiler::get_repl_input() {
        std::string result;
        bool        first_line = true;

        while (true) {
            const char *input;
            if (first_line) {
                input = m_repl->readline(get_prompt());
                first_line = false;
            } else {
                input = m_repl->readline("      ");
            }

            if (!input) return "";

            std::string line(input);

            if (!result.empty()) { result += "\n"; }
            result += line;

            if (m_soot.get_reader().is_expression_complete(result)) {
                m_repl->add_to_history(result);
                return result;
            }
        }
    }

    // Interpret a script and optionally print the result.
    ReplStatus Compiler::interpret_and_print(const std::string &script) {
        auto result = interpret(script);
        if (m_config.compiler.dump_ast) { lg::info("=> {}", result.print()); }
        return ReplStatus::OK;
    }

    // Compile a REPL expression, print a listing, and register the module.
    ReplStatus Compiler::compile_and_report(script::Object forms) {
        try {
            if (forms.is_null()) {
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                           "; ERROR: failed to read input\n");
                return ReplStatus::ERR;
            }

            // Walk the top-level forms. For each:
            //   * if it is a REPL command (head symbol in repl_commands()),
            //     dispatch it and continue;
            //   * otherwise compile it as a single-form module.
            auto cur = forms;
            while (cur.is_pair()) {
                auto form = cur.as_pair()->car;
                cur = cur.as_pair()->cdr;

                if (auto status = try_handle_repl_command(form)) {
                    if (*status == ReplStatus::WANT_EXIT) return ReplStatus::WANT_EXIT;
                    if (*status == ReplStatus::WANT_RELOAD) return ReplStatus::WANT_RELOAD;
                    if (*status == ReplStatus::ERR) return ReplStatus::ERR;
                    continue;
                }

                // Compile this single form.
                auto one = script::Object::make_pair(form, script::Object::make_null());
                auto result = compile_file_repl(one, "<repl>");
                if (!result) {
                    fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; ERROR: {}\n",
                               result.error());
                    return ReplStatus::ERR;
                }

                // Record the form in the session ONLY after it compiled successfully.
                // A failed input is not part of the session: replaying it in
                // save_session would fail again, and the user has no way to remove it
                // from the session without restarting the REPL. This also matches
                // rule 5: the session is the history of successful inputs, not a
                // log of everything typed.
                m_session_forms.push_back(form);

                if (!*result) {
                    fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                               "; OK (types/enums registered)\n");
                    continue;
                }

                if (!result) {
                    fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; ERROR: {}\n",
                               result.error());
                    return ReplStatus::ERR;
                }

                if (!*result) {
                    fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                               "; OK (types/enums registered)\n");
                    continue;
                }

                if (m_config.compiler.print_listing) { print_listing(**result); }

                bool loaded = m_runtime->load_module(std::move(**result));
                if (!loaded) {
                    fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold,
                               "; WARN: failed to register module in runtime '{}'\n",
                               m_runtime->name());
                }

                if (auto *local = dynamic_cast<LocalVmRuntime *>(m_runtime.get())) {
                    const std::string &logical = local->last_logical_name();
                    if (!logical.empty()) {
                        auto run_result = m_runtime->run_top_level(logical);
                        if (!run_result.ok) {
                            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                                       "; ERROR: {}\n", run_result.error);
                            return ReplStatus::ERR;
                        }
                        fmt::print(fg(fmt::color::cyan), "; => {}\n", run_result.value.to_string());
                    }
                }

                fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "; OK\n");
            }

            return ReplStatus::OK;
        } catch (const std::exception &e) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; EXCEPTION: {}\n",
                       e.what());
            return ReplStatus::ERR;
        }
    }

    // Try to interpret; on failure, fall back to compilation.
    ReplStatus Compiler::try_interpret_then_compile(script::Object form) {
        try {
            auto result = interpret(form);
            if (m_config.compiler.dump_ast) { lg::info("=> {}", result.print()); }
            return ReplStatus::OK;
        } catch (const std::exception &e) {
            lg::debug("Interpret failed: {}, trying compilation", e.what());
            return compile_and_report(form);
        }
    }

    // Interpret a string by parsing it first.
    script::Object Compiler::interpret(const std::string &input) {
        auto forms = m_soot.get_reader().read_from_string(input, false, "<repl>");
        return interpret(forms);
    }

    // Interpret already-parsed forms in the global SOOT environment.
    script::Object Compiler::interpret(const script::Object &forms) {
        if (m_config.compiler.dump_ast) { lg::info("AST: {}", forms.print()); }

        script::Object result = script::Object::make_none();
        try {
            auto env = m_soot.get_global_environment();

            if (forms.is_pair()) {
                for_each_in_list(forms, [&](const script::Object &o) {
                    result = m_soot.eval_form(o, env.as_env_ptr());
                });
            } else {
                result = m_soot.eval_form(forms, env.as_env_ptr());
            }

            printf("%s\n", result.print().c_str());
            return result;

        } catch (script::ExitException &e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nExit: {}\n", e.what());
            exit(e.exit_code);
        } catch (script::EvalException &e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nError:");
            fmt::print("Error: {}", e.full_report(m_soot.get_reader()));
        } catch (const std::exception &e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nError: {}\n", e.what());
        }
        return result;
    }

    // ========== Environment management ==========

    void Compiler::set_global(const std::string &name, const script::Object &value) {
        m_soot.define_global(name.c_str(), value);
    }

    script::Object Compiler::get_global(const std::string &name) {
        return m_soot.get_global(Object::intern(&m_soot.symbol_table(), name.c_str()));
    }

    void Compiler::reload_environment() {
        lg::info("Reloading environment...");
        load_user_profile();
        lg::info("Environment reloaded");
    }

    // ========== Private methods ==========

    // Set up the REPL wrapper. If a wrapper was already provided to the
    // constructor, reuse it; otherwise create a fresh one.
    void Compiler::setup_repl() {
        if (!m_repl) { m_repl = std::make_unique<REPL::Wrapper>(m_platform); }

        // Give (opt ...) access to the interpreter, so that
        // (opt key) can build script::Objects. Without this, every
        // (opt key) prints (unavailable).
        common::set_repl_interpreter(&m_soot);

        m_repl->username = m_config.compiler.user_profile;
        m_repl->init_settings();
        m_repl->load_history();
    }

    void Compiler::color_binary_file(std::unique_ptr<BinaryFile> & /*binary*/) {
        // Register allocation and optimizations would go here.
        if (m_config.compiler.dump_asm) {
            // binary->print_asm();
        }
    }

    std::vector<uint8_t> Compiler::codegen_binary(BinaryFile *binary) {
        // Final code generation step (not implemented yet).
        (void)binary;
        return {};
    }

    // Locate a file: first check as-is, then try every search path from the
    // configuration. Returns the original name if nothing is found.
    std::string Compiler::find_file(const std::string &filename) {
        namespace fs = std::filesystem;

        if (fs::exists(filename)) { return filename; }

        for (const auto &dir : m_config.compiler.search_paths) {
            fs::path candidate = fs::path(dir) / filename;
            if (fs::exists(candidate)) { return candidate.string(); }
        }

        return filename;
    }

    std::string Compiler::read_file_content(const std::string &filename) {
        std::string   path = find_file(filename);
        std::ifstream file(path);
        if (!file.is_open()) { throw std::runtime_error("Cannot open file: " + path); }

        std::string content;
        file.seekg(0, std::ios::end);
        content.resize(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(&content[0], content.size());

        // Strip UTF-8 BOM if present.
        if (content.size() >= 3 && static_cast<uint8_t>(content[0]) == 0xEF &&
            static_cast<uint8_t>(content[1]) == 0xBB && static_cast<uint8_t>(content[2]) == 0xBF) {
            content = content.substr(3);
        }

        return content;
    }

    void Compiler::print_error(const std::string &context, const std::exception &e) {
        fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "{}: {}\n", context, e.what());
    }

    void Compiler::print_warning(const std::string &warning) {
        fmt::print(fg(fmt::color::yellow), "Warning: {}\n", warning);
    }

    // ===============================================================
    // REPL callbacks
    // ===============================================================

    replxx::Replxx::completions_t
    Compiler::find_symbols_or_object_file_by_prefix(const std::string &context, int &context_len,
                                                    const std::vector<std::string> &examples) {
        replxx::Replxx::completions_t matches;

        if (context.empty()) {
            context_len = 0;
            return matches;
        }

        context_len = static_cast<int>(context.length());

        // Add matching examples.
        for (const auto &ex : examples) {
            if (ex.find(context) == 0) { matches.emplace_back(ex); }
        }

        // Add matching symbols from the interpreter.
        auto symbols = m_soot.get_all_symbols_matching(context);
        for (const auto &sym : symbols) { matches.emplace_back(sym); }

        return matches;
    }

    replxx::Replxx::hints_t
    Compiler::find_hints_by_prefix(const std::string &context, int &context_len,
                                   replxx::Replxx::Color          &color,
                                   const std::vector<std::string> &examples) {
        replxx::Replxx::hints_t hints;

        if (context.empty()) {
            context_len = 0;
            return hints;
        }

        context_len = static_cast<int>(context.length());
        color = replxx::Replxx::Color::BRIGHTCYAN;

        if (context == "defun" || context == "defmacro") {
            hints.push_back(" (name args body...)");
        } else if (context == "if") {
            hints.push_back(" (test then else)");
        } else if (context == "when") {
            hints.push_back(" (test body...)");
        } else if (context == "cond") {
            hints.push_back(" (clause...)");
        } else if (context == "let" || context == "let*") {
            hints.push_back(" ((var val)...) body...");
        } else if (context == "lambda") {
            hints.push_back(" (args body...)");
        } else if (context == "define") {
            hints.push_back(" (name value)");
        } else if (context == "set!") {
            hints.push_back(" (var value)");
        } else if (context == "quote") {
            hints.push_back(" (expr)");
        } else if (context == "load" || context == "compile") {
            hints.push_back(" \"filename\"");
        } else if (context == "deftype") {
            hints.push_back(" (name parent (field...))");
        } else if (context == "defenum") {
            hints.push_back(" (name values...)");
        } else if (context == "sc-run") {
            hints.push_back(" (name arg...)");
        } else if (context == "sc-eval") {
            hints.push_back(" (form)");
        } else if (context == "save-session") {
            hints.push_back(" \"name\"");
        }

        if (hints.empty()) {
            for (const auto &ex : examples) {
                if (ex.find(context) == 0 && ex != context) {
                    hints.push_back(ex.substr(context.length()));
                }
            }
        }

        return hints;
    }

    void Compiler::repl_coloring(const std::string &input, replxx::Replxx::colors_t &colors) {
        // Some replxx builds pass a buffer that is exactly input.size() wide;
        // others may pass one that is shorter (or empty) on the first call.
        if (colors.size() < input.size()) {
            colors.resize(input.size(), replxx::Replxx::Color::DEFAULT);
        }

        bool in_string = false;
        bool in_comment = false;

        for (size_t i = 0; i < input.size(); i++) {
            const char c = input[i];

            if (c == '"' && !in_comment) {
                in_string = !in_string;
                colors[i] = replxx::Replxx::Color::GREEN;
            } else if (c == ';' && !in_string) {
                in_comment = true;
                colors[i] = replxx::Replxx::Color::BRIGHTCYAN;
            } else if (in_comment) {
                colors[i] = replxx::Replxx::Color::BRIGHTCYAN;
            } else if (in_string) {
                colors[i] = replxx::Replxx::Color::GREEN;
            } else if (c == '(' || c == ')') {
                colors[i] = replxx::Replxx::Color::BRIGHTBLUE;
            } else if (std::isdigit(static_cast<unsigned char>(c)) ||
                       (c == '-' && i + 1 < input.size() &&
                        std::isdigit(static_cast<unsigned char>(input[i + 1])))) {
                colors[i] = replxx::Replxx::Color::YELLOW;
            }
        }
    }

    bool Compiler::is_soot_macro(const std::string &name) {
        auto sym = m_soot.intern(name.c_str());
        return try_getting_macro_from_soot(m_soot, sym, nullptr);
    }

    script::Object Compiler::expand_soot_macro(const script::Object &form) {
        return expand_soot_macro_for_compiler(m_soot, form);
    }

    // Synthesize a unique name for an anonymous lambda.
    //
    // Called by NodeBuilder::build_call when the head of a call is a
    // lambda form rather than a symbol. The generated name is used as
    // the entry name inside the binary and as the binding in the
    // enclosing FileNode's symbol table, so it must be unique within
    // that file. Because each file is compiled by a single Compiler
    // instance, a per-instance counter is sufficient.
    //
    // The format "<base>::lambda#<n>" is deliberately not a valid SOOT
    // symbol from the reader's point of view: it contains ':' and '#',
    // which the reader would reject. This guarantees that a synthesized
    // name can never collide with a name the user wrote, even if the
    // user's base string happens to match an existing function.
    //
    // @param base  Human-readable prefix, typically the source file's
    //              path. May be empty; in that case "<anon>" is used.
    // @return      A name of the form "<base>::lambda#<n>".
    std::string Compiler::make_unique_lambda_name(const std::string &base) {
        // Fall back to a stable placeholder when the caller has no
        // useful base string. Using "<anon>" rather than "" keeps the
        // resulting name readable in disassembly and logs.
        const std::string &effective_base = base.empty() ? "<anon>" : base;

        // Post-increment: the first lambda gets #0, the second #1, and
        // so on. The counter is never reset, so names are unique for
        // the lifetime of this Compiler instance.
        return fmt::format("{}::lambda#{}", effective_base, m_lambda_counter++);
    }

} // namespace sootc