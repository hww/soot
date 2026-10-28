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
#include "util/Log.hpp"

namespace sootc {

    // Construct the compiler and bring it to a usable state:
    //   1. load the SOOT interpreter prelude (lib.st)  -> m_soot
    //   2. compile the SOOTC compiler prelude (lib.sc) -> m_ts / m_soot macros
    //   3. set up the REPL, if the mode requires it
    //   4. load the user profile (if any)
    //   5. register C++-side SOOT forms that need access to the compiler
    Compiler::Compiler(const CompilationOptions          comp_options,
                       const std::optional<REPL::Config> repl_config,
                       const std::string &user_profile, std::unique_ptr<REPL::Wrapper> repl)
        : m_ts(TypeSystem::instance()), m_platform(comp_options.platform),
          m_soot(user_profile, false, true, comp_options.platform), m_repl(std::move(repl)) {
        (void)repl_config;
        m_config = comp_options;
        sync_type_verbose(); 

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
        if (m_config.runtime_url.empty()) {
            // Convert the string search_paths from CompilationOptions
            // into filesystem::path for CarbonConfig. The runtime uses
            // these to find .bin files when a script calls
            // (load-module "name").
            std::vector<std::filesystem::path> runtime_search_paths;
            runtime_search_paths.reserve(m_config.search_paths.size());
            for (const auto &s : m_config.search_paths) { runtime_search_paths.emplace_back(s); }
            m_runtime = std::make_unique<LocalVmRuntime>(std::move(runtime_search_paths));
        } else if (m_config.runtime_url == "null") {
            m_runtime = std::make_unique<NullRuntime>();
        } else {
            m_runtime = std::make_unique<RemoteRuntime>(m_config.runtime_url);
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
        if (!m_config.sidbase_path.empty()) {
            std::error_code ec;
            const bool      exists = std::filesystem::exists(m_config.sidbase_path, ec);

            if (ec) {
                lg::warn("SIDBase: cannot stat '{}': {} — starting empty", m_config.sidbase_path,
                         ec.message());
            } else if (!exists) {
                lg::info("SIDBase: '{}' not found — will be created on exit",
                         m_config.sidbase_path);
            } else {
                const bool loaded = carbon::StringIdManager::instance().load_dconstruct_sidbase(
                    m_config.sidbase_path);

                if (loaded) {
                    lg::info("SIDBase: loaded {} entries from '{}'",
                             carbon::StringIdManager::instance().size(), m_config.sidbase_path);
                } else {
                    lg::warn("SIDBase: failed to load '{}' — starting empty",
                             m_config.sidbase_path);
                }
            }
        } else {
            lg::info("SIDBase: no path configured — using in-memory registry only");
        }

        // 1. SOOT prelude -> interpreter (lib.st).
        load_soot_prelude();

        // 2. SOOTC prelude -> compiler (lib.sc).
        load_soc_prelude();

        if (m_config.mode != CompilerMode::COMPILE_ONLY) { setup_repl(); }

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

    // ========== Compilation ==========


    std::expected<std::unique_ptr<BinaryFile>, std::string>
    Compiler::compile_file(script::Object &forms, const std::string &filename) {
        m_current_file = filename;
        auto result = compile_internal(forms, filename, /*is_repl=*/false);

        if (result && m_config.debug_print_ir) {
            lg::info("Compilation successful for: {}", filename);
            BinaryFileInspector inspector(result->get());
            inspector.inspect();
        }
        return result;
    }

    std::expected<std::unique_ptr<BinaryFile>, std::string>
    Compiler::compile_file(const std::filesystem::path &path) {
        std::string content = read_file_content(path.string());
        auto        forms = m_soot.get_reader().read_from_string(content, false, path.string());
        if (forms.is_null()) {
            return std::unexpected("Failed to read or parse file: " + path.string());
        }
        return compile_file(forms, path.string());
    }
    std::expected<std::unique_ptr<BinaryFile>, std::string>
    Compiler::compile_file_repl(script::Object &forms, const std::string &filename) {
        m_current_file = filename;
        auto result = compile_internal(forms, filename, /*is_repl=*/true);

        if (result && m_config.debug_print_ir) {
            lg::info("Compilation successful for: {}", filename);
            BinaryFileInspector inspector(result->get());
            inspector.inspect();
        }
        return result;
    }

    // (require "path/to/file.sc")
    //
    // Implements require as a language form. Called by NodeBuilder when
    // it sees `require` at the head of a form during compilation.
    //
    ////// The file is resolved against the compiler's search_paths. If the
    // resolved path has already been loaded (in this Compiler instance,
    // via an earlier require or a user action), the call is a no-op.
    // Otherwise the file is compiled and the resulting module is
    // registered in the active runtime.
    //
    // See the header comment on Compiler::require_file for the
    // rationale, including the handling of cyclic requires.
    bool Compiler::require_file(const script::Object &form, const std::string &path) {
        (void)form;
        // ------------------------------------------------------------------
        // 1. Resolve the path against search_paths.
        // ------------------------------------------------------------------
        namespace fs = std::filesystem;

        // Resolve `path` against PROJECT and search_paths. An absolute path
        // is taken as-is. A relative path is tried:
        //   1. as-is (CWD) — for backward compatibility and for paths the
        //      user typed explicitly;
        //   2. PROJECT / path;
        //   3. PROJECT / dir / path for each search_path.
        //
        // This is the same resolution order as save_session, which always
        // writes to PROJECT / <dir> / <name>.bin. Without it, a file saved
        // by save_session cannot be found by require.
        std::string resolved;
        bool        found = false;
        if (fs::path(path).is_absolute()) {
            resolved = path;
            found = fs::exists(resolved);
        } else {
            // 1. CWD
            if (fs::exists(path)) {
                resolved = path;
                found = true;
            }
            // 2. PROJECT
            if (!found) {
                fs::path candidate = file_util::get_path(file_util::PathType::PROJECT) / path;
                if (fs::exists(candidate)) {
                    resolved = candidate.string();
                    found = true;
                }
            }

            // ============================================================
            // 2b. Relative to the directory of the file currently being
            //     compiled.
            //
            //     A (require "sibling.sc") is naturally resolved against
            //     the dependent file's own directory, before the global
            //     search_paths are consulted. This mirrors #include "..."
            //     in C/C++ and require() in Node.js: the dependency lives
            //     next to its dependent, not in a global search root.
            //
            //     Without this, a file in a subdirectory (e.g.
            //     soot_src/tests/foo.sc) cannot require a sibling
            //     (soot_src/tests/bar.sc) unless soot_src/tests/ is
            //     explicitly listed in search_paths. That is surprising
            //     and forces the user to pass --source-root for a case
            //     that should Just Work.
            //
            //     m_current_file is set by compile_file / compile_file_repl
            //     before this function is reached, so it always points at
            //     the .sc file whose form contains the (require ...).
            // ============================================================
            if (!found && !m_current_file.empty()) {
                fs::path candidate = fs::path(m_current_file).parent_path() / path;
                if (fs::exists(candidate)) {
                    resolved = candidate.string();
                    found = true;
                }
            }

            // 3. PROJECT / search_path
            if (!found) {
                for (const auto &dir : m_config.search_paths) {
                    fs::path candidate =
                        file_util::get_path(file_util::PathType::PROJECT) / dir / path;
                    if (fs::exists(candidate)) {
                        resolved = candidate.string();
                        found = true;
                        break;
                    }
                }
            }
        }
        if (!found) {
            throw CompilerError("Compiler::require_file")
                .expected("a file that exists in search_paths")
                .got(fmt::format("'{}'", path))
                .note(fmt::format("search_paths: {}", fmt::join(m_config.search_paths, ", ")))
                .note(fmt::format("PROJECT: {}",
                                  file_util::get_path(file_util::PathType::PROJECT).string()));
        }

        // ------------------------------------------------------------------
        // 2. Normalize the path so that two different spellings of the
        //    same file resolve to the same key in m_loaded_files.
        // ------------------------------------------------------------------
        std::error_code ec;
        fs::path        canonical = fs::weakly_canonical(resolved, ec);
        if (ec) {
            // weakly_canonical can fail if the path does not exist, but
            // we already checked fs::exists above, so this is rare. Fall
            // back to the resolved path as-is.
            canonical = resolved;
        }

        // ------------------------------------------------------------------
        // 3. Skip if already loaded.
        // ------------------------------------------------------------------
        if (m_loaded_files.count(canonical)) {
            lg::debug("require: '{}' already loaded — skipping", canonical.string());
            return true;
        }

        // Mark as loaded BEFORE compiling. This is what makes cyclic
        // requires terminate: if the file we are about to compile
        // requires itself (directly or transitively), the inner call
        // sees the key in m_loaded_files and returns immediately.
        m_loaded_files.insert(canonical);

        // ------------------------------------------------------------------
        // 4. Compile and load.
        // ------------------------------------------------------------------
        lg::info("require: loading '{}'", canonical.string());

        auto result = compile_file(canonical);
        if (!result) {
            // Compilation failed. Remove the marker so that a later
            // require of the same file can be retried after the user
            // fixes the problem.
            m_loaded_files.erase(canonical);
            throw CompilerError("Compiler::require_file")
                .expected("a compilable file")
                .got(fmt::format("'{}'", canonical.string()))
                .caused_by(result.error());
        }

        // `*result` is the unique_ptr<BinaryFile>. It is empty when the
        // file contained only compile-time declarations (deftype /
        // defenum / defconstant). That is a normal outcome: the types
        // are already registered in TypeSystem, so there is nothing to
        // load into the runtime.
        if (!*result) {
            lg::debug("require: '{}' produced no binary (compile-time only)", canonical.string());
            return true;
        }

        bool loaded = m_runtime->load_module(std::move(**result));
        if (!loaded) {
            m_loaded_files.erase(canonical);
            throw CompilerError("Compiler::require_file")
                .expected("the runtime to accept the module")
                .got(fmt::format("runtime '{}' rejected '{}'", m_runtime->name(),
                                 canonical.string()));
        }

        lg::info("require: loaded '{}'", canonical.string());
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
                        // second TypeDeclarationNode. The owner FileNode
                        // already holds the SsType with every method that
                        // defmethod attached to it. Adding a fresh
                        // TypeDeclarationNode here would emit a second
                        // SsType with m_pMethods[*].m_pLambda == nullptr,
                        // and load_module would overwrite Globals[<type>]
                        // with that empty table. CallMethod would then
                        // dereference null.
                        //
                        // Instead we drop the new TypeDeclarationNode.
                        // TypeSystem already has the type; the owner
                        // FileNode is already loaded; nothing needs to
                        // change.
                        const std::string &type_name = td->type().base_type();
                        FileNode          *existing_owner = nullptr;
                        for (auto &[oname, onode] : m_file_nodes) {
                            (void)oname;
                            if (onode.get() == file_node) continue;
                            for (auto &child : onode->children()) {
                                if (auto *other_td =
                                        dynamic_cast<TypeDeclarationNode *>(child.get())) {
                                    if (other_td->type().base_type() == type_name) {
                                        existing_owner = onode.get();
                                        break;
                                    }
                                }
                            }
                            if (existing_owner) break;
                        }

                        if (existing_owner) {
                            lg::info("compile_internal: '{}' already declared in '{}', skipping",
                                     type_name, existing_owner->name());
                            // node is a unique_ptr; going out of scope frees it.
                        } else {
                            file_node->add_child(std::move(node));
                        }
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
                if (m_config.debug_print_ast) inspect_ast(node.get());
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

            if (m_config.debug_print_ir) { element.dump(); }

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

            if (m_config.debug_print_asm) { color_binary_file(binary); }

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
            if (m_config.debug_print_ast && file_node) { inspect_ast(file_node); }
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

    // The set of names recognised as REPL commands. Kept as a
    // function-local static so it is built exactly once per process.
    const std::set<std::string> &Compiler::repl_commands() {
        static const std::set<std::string> kCommands = {"help",
                                                        "clear",
                                                        "e",
                                                        "reload",
                                                        "sizes",
                                                        "inspect-env",
                                                        "inspect-env-soots",
                                                        "has-macro",
                                                        "list-symbols",
                                                        "save-session",
                                                        "save-module", 
                                                        "sc-run",
                                                        "sc-eval",
                                                        "arg-set",
                                                        "arg-get",
                                                        "arg-ls",
                                                        "gcls",
                                                        "gc",
                                                        "ts-info",
                                                        "ast"};
        return kCommands;
    }

    // If `input` parses to a REPL command form, dispatch it and return
    // its status. Otherwise return std::nullopt so the caller can fall
    // through to the normal compile pipeline.
    std::optional<ReplStatus> Compiler::try_handle_repl_command(const script::Object &form) {
        if (!form.is_pair()) return std::nullopt;
        const auto &head = form.as_pair()->car;
        if (!head.is_symbol()) return std::nullopt;
        const std::string name = head.as_symbol().name_ptr;
        if (!repl_commands().count(name)) return std::nullopt;
        return dispatch_repl_command(name, form);
    }

    ReplStatus Compiler::dispatch_repl_command(const std::string    &name,
                                               const script::Object &form) {
        if (name == "help") return cmd_help(form);
        if (name == "clear") return cmd_clear(form);
        if (name == "e") return cmd_e(form);
        if (name == "reload") return cmd_reload(form);
        if (name == "sizes") return cmd_sizes(form);
        if (name == "inspect-env") return cmd_inspect_env(form);
        if (name == "inspect-env-soots") return cmd_inspect_env_soots(form);
        if (name == "has-macro") return cmd_has_macro(form);
        if (name == "list-symbols") return cmd_list_symbols(form);
        if (name == "save-session") return cmd_save_session(form);
        if (name == "save-module") return cmd_save_module(form);
        if (name == "sc-run") return cmd_sc_run(form);
        if (name == "sc-eval") return cmd_sc_eval(form);
        if (name == "arg-set") return cmd_arg_set(form);
        if (name == "arg-get") return cmd_arg_get(form);
        if (name == "arg-ls") return cmd_arg_list(form);
        if (name == "gcls") return cmd_gc_list(form);
        if (name == "gc") return cmd_gc_collect(form);
        if (name == "ts-info") return cmd_ts_info(form);
        if (name == "ast") return cmd_ast(form);

        fmt::print(fg(fmt::color::crimson), "; ERROR: unknown REPL command '{}'\n", name);
        return ReplStatus::ERR;
    }

    // ===============================================================
    // REPL command handlers
    // ===============================================================

    ReplStatus Compiler::cmd_help(const script::Object &form) {
        (void)form;
        if (m_repl) m_repl->print_help_message();
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_clear(const script::Object &form) {
        (void)form;
        if (m_repl) m_repl->clear_screen();
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_e(const script::Object &form) {
        (void)form;
        return ReplStatus::WANT_EXIT;
    }

    ReplStatus Compiler::cmd_reload(const script::Object &form) {
        (void)form;
        return ReplStatus::WANT_RELOAD;
    }

    ReplStatus Compiler::cmd_sizes(const script::Object &form) {
        (void)form;
        carbon::print_all_struct_sizes();
        return ReplStatus::OK;
    }

    // Dump the compiler-side environment (*soot-env*).
    //
    // *soot-env* is the environment where lib.sc registers its macros.
    // try_getting_macro_from_soot consults only this environment, so
    // whatever appears here is what the compiler sees as macros.
    ReplStatus Compiler::cmd_inspect_env(const script::Object &form) {
        (void)form;
        auto sootc_env = m_soot.get_soot_environment().as_env_ptr();

        fmt::print("\n=== Compiler environment (*soot-env*) ===\n");

        const auto &entries = sootc_env->vars.get_all_entries();
        std::size_t used = 0;
        for (const auto &entry : entries) {
            if (!entry.key) continue;
            ++used;

            const char *kind = "value";
            if (entry.value.is_macro()) kind = "macro";
            else if (entry.value.is_function())
                kind = "function";
            else if (entry.value.is_special_form())
                kind = "special-form";
            else if (entry.value.is_primitive())
                kind = "primitive";

            fmt::print("  {:<40} {}\n", entry.key, kind);
        }

        fmt::print("=== End ({} slots, {} used) ===\n\n", entries.size(), used);
        return ReplStatus::OK;
    }

    // Dump the interpreter-side environment (*global-env*).
    //
    // *global-env* is where lib.st and the interpreter register their
    // macros and built-in functions. The compiler does NOT consult this
    // environment for macro dispatch.
    ReplStatus Compiler::cmd_inspect_env_soots(const script::Object &form) {
        (void)form;
        auto global_env = m_soot.get_global_environment().as_env_ptr();

        fmt::print("\n=== Interpreter environment (*global-env*) ===\n");

        const auto &entries = global_env->vars.get_all_entries();
        std::size_t used = 0;
        for (const auto &entry : entries) {
            if (!entry.key) continue;
            ++used;

            const char *kind = "value";
            if (entry.value.is_macro()) kind = "macro";
            else if (entry.value.is_function())
                kind = "function";
            else if (entry.value.is_special_form())
                kind = "special-form";
            else if (entry.value.is_primitive())
                kind = "primitive";

            fmt::print("  {:<40} {}\n", entry.key, kind);
        }

        fmt::print("=== End ({} slots, {} used) ===\n\n", entries.size(), used);
        return ReplStatus::OK;
    }

    // Check whether `name` is a macro visible to the compiler, i.e. a
    // macro registered in *soot-env*.
    ReplStatus Compiler::cmd_has_macro(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: (has-macro \"name\") requires a name\n");
            return ReplStatus::ERR;
        }
        const auto &name_obj = rest.as_pair()->car;
        if (!name_obj.is_symbol() && !name_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (has-macro ...) name must be symbol or string\n");
            return ReplStatus::ERR;
        }
        const std::string name = name_obj.to_std_string();
        const bool        is_macro = is_soot_macro(name);

        fmt::print(fg(is_macro ? fmt::color::green : fmt::color::yellow),
                   "; has-macro \"{}\" => {}\n", name, is_macro ? "true" : "false");
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_list_symbols(const script::Object &form) {
        (void)form;

        carbon::Carbon *carbon = carbon::Carbon::current();
        if (!carbon) {
            fmt::print(fg(fmt::color::yellow), "; WARN: no active Carbon context\n");
            return ReplStatus::OK;
        }

        auto symbols = carbon->globals()->all_symbols();
        for (auto &s : symbols) { fmt::print("  {}\n", s.to_cstring()); }
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_save_session(const script::Object &form) {
        std::string session_name = m_session_name;

        auto rest = form.as_pair()->cdr;
        if (rest.is_pair() && rest.as_pair()->car.is_string()) {
            session_name = rest.as_pair()->car.to_std_string();
        }

        try {
            if (save_session(session_name, ".")) {
                fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                           "; saved session as {}.bin and {}.h\n", session_name, session_name);
            } else {
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                           "; ERROR: failed to save session\n");
                return ReplStatus::ERR;
            }
        } catch (const std::exception &e) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; ERROR: {}\n", e.what());
            return ReplStatus::ERR;
        }
        return ReplStatus::OK;
    }
    // (save-module "path/to/foo.sc" ["out_dir"])
    //
    // Compile a .sc file as its own module and save it. This is the
    // module-level counterpart to save-session: it writes what the file
    // contains, not what the REPL has accumulated.
    ReplStatus Compiler::cmd_save_module(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (save-module \"path/to/file.sc\" [\"out_dir\"]) "
                       "requires a path\n");
            return ReplStatus::ERR;
        }

        const auto &path_obj = rest.as_pair()->car;
        if (!path_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (save-module ...) path must be a string\n");
            return ReplStatus::ERR;
        }

        std::string path = path_obj.to_std_string();
        std::string out_dir = "build";

        auto after_path = rest.as_pair()->cdr;
        if (after_path.is_pair()) {
            const auto &dir_obj = after_path.as_pair()->car;
            if (!dir_obj.is_string()) {
                fmt::print(fg(fmt::color::crimson),
                           "; ERROR: (save-module ...) out_dir must be a string\n");
                return ReplStatus::ERR;
            }
            out_dir = dir_obj.to_std_string();
        }

        if (save_module(path, out_dir)) {
            fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "; OK: module '{}' saved\n",
                       path);
            return ReplStatus::OK;
        }
        return ReplStatus::ERR;
    }
    // (sc-run name arg...)
    //
    // Execute a compiled function in the active runtime.
    //
    // Argument types are inferred from the SOOT object kind:
    //   integer   -> i64
    //   float     -> f32
    //   string    -> std::string
    //   symbol    -> StringId
    //
    // Example: (sc-run gcd 48 18)
    ReplStatus Compiler::cmd_sc_run(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: (sc-run name arg...) requires a name\n");
            return ReplStatus::ERR;
        }

        const auto &name_obj = rest.as_pair()->car;
        if (!name_obj.is_symbol() && !name_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (sc-run ...) name must be symbol or string\n");
            return ReplStatus::ERR;
        }
        const std::string name = name_obj.to_std_string();

        // Parse arguments as SOOT objects and convert to Variants.
        std::vector<carbon::Variant> vm_args;
        auto                         args_rest = rest.as_pair()->cdr;
        while (args_rest.is_pair()) {
            const auto &o = args_rest.as_pair()->car;
            if (o.is_integer()) {
                vm_args.push_back(carbon::Variant(static_cast<i64>(o.as_integer())));
            } else if (o.is_float()) {
                vm_args.push_back(carbon::Variant(static_cast<f32>(o.as_float())));
            } else if (o.is_string()) {
                vm_args.push_back(carbon::Variant(o.to_std_string()));
            } else if (o.is_symbol()) {
                vm_args.push_back(carbon::Variant(carbon::StringId(o.as_symbol().name_ptr)));
            } else {
                fmt::print(fg(fmt::color::crimson),
                           "; ERROR: (sc-run ...) unsupported argument kind: {}\n", o.print());
                return ReplStatus::ERR;
            }
            args_rest = args_rest.as_pair()->cdr;
        }

        // Hand the call to the active runtime.
        auto result = m_runtime->call(name, vm_args);
        if (!result.ok) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; ERROR: {}\n",
                       result.error);
            return ReplStatus::ERR;
        }

        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "; {} => {}\n", name,
                   result.value.to_string());
        return ReplStatus::OK;
    }

    // (sc-eval expr)
    //
    // Evaluate a SOOT expression in the interpreter's global environment
    // and print the result. This is the compiler-side analogue of the
    // old `:soot` REPL command.
    ReplStatus Compiler::cmd_sc_eval(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (sc-eval <expr>) requires an expression\n");
            return ReplStatus::ERR;
        }
        try {
            auto result =
                m_soot.eval_form(rest.as_pair()->car, m_soot.get_global_environment().as_env_ptr());
            fmt::print(fg(fmt::color::green), "{}\n", result.print());
        } catch (const std::exception &e) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: {}\n", e.what());
            return ReplStatus::ERR;
        }
        return ReplStatus::OK;
    }

    // (arg-set "key" value)
    //
    // Set a compiler runtime parameter. The parameter is stored
    // directly in m_config (CompilationOptions), which is the single
    // source of truth for every compiler option — the same struct
    // that the CLI populates at startup and that compile_and_report
    // reads when it decides whether to print the listing, the IR, etc.
    //
    // Changing a parameter here takes effect immediately for every
    // subsequent compilation in this session.
    //
    // Recognised keys (see also cmd_arg_get):
    //   "print-listing"  bool   — print the binary listing after each compile
    //   "print-ir"       bool   — print the IR
    //   "print-ast"      bool   — print the AST
    //   "print-asm"      bool   — print the generated assembly
    //   "log-level"      string — log level: "debug" | "info" | "warn" | "error"
    //
    // Read-only keys (settable only from the CLI):
    //   "sidbase", "runtime", "search-paths", "platform",
    //   "mode", "user-profile" — query with (arg-get ...) but not settable.
    //
    // Examples:
    //   (arg-set "print-listing" #t)
    //   (arg-set "log-level" "debug")
    ReplStatus Compiler::cmd_arg_set(const script::Object &form) {
        // --- Parse the key and value ---
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (arg-set \"key\" value) requires a key and a value\n");
            return ReplStatus::ERR;
        }
        const auto &key_obj = rest.as_pair()->car;
        if (!key_obj.is_string() && !key_obj.is_symbol()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (arg-set ...) key must be string or symbol\n");
            return ReplStatus::ERR;
        }

        auto after_key = rest.as_pair()->cdr;
        if (!after_key.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (arg-set \"{}\" value) requires a value\n",
                       key_obj.to_std_string());
            return ReplStatus::ERR;
        }

        const std::string key = key_obj.to_std_string();
        const auto       &value = after_key.as_pair()->car;

        // --- Helpers ---
        //
        // SOOT has no native bool type; #t / #f are the canonical
        // truthy values. We accept any symbol and treat "#t" as true,
        // anything else as false, so that (arg-set "x" #f) works and
        // (arg-set "x" whatever) is not silently truthy.
        auto as_bool = [](const script::Object &o) -> bool {
            return o.is_symbol() && o.as_symbol() == "#t";
        };

        // --- Dispatch ---
        if (key == "print-listing") {
            m_config.print_listing = as_bool(value);
            fmt::print(fg(fmt::color::green), "; arg-set \"print-listing\" => {}\n",
                       m_config.print_listing ? "#t" : "#f");
            return ReplStatus::OK;
        }
        if (key == "print-ir") {
            m_config.debug_print_ir = as_bool(value);
            fmt::print(fg(fmt::color::green), "; arg-set \"print-ir\" => {}\n",
                       m_config.debug_print_ir ? "#t" : "#f");
            return ReplStatus::OK;
        }
        if (key == "print-ast") {
            m_config.debug_print_ast = as_bool(value);
            fmt::print(fg(fmt::color::green), "; arg-set \"print-ast\" => {}\n",
                       m_config.debug_print_ast ? "#t" : "#f");
            return ReplStatus::OK;
        }
        if (key == "print-asm") {
            m_config.debug_print_asm = as_bool(value);
            fmt::print(fg(fmt::color::green), "; arg-set \"print-asm\" => {}\n",
                       m_config.debug_print_asm ? "#t" : "#f");
            return ReplStatus::OK;
        }
        if (key == "log-level") {
            if (!value.is_string() && !value.is_symbol()) {
                fmt::print(fg(fmt::color::crimson),
                           "; ERROR: \"log-level\" expects a string (debug|info|warn|error)\n");
                return ReplStatus::ERR;
            }
            m_config.log_level = value.to_std_string();
            sync_type_verbose(); 
            fmt::print(fg(fmt::color::green), "; arg-set \"log-level\" => \"{}\"\n",
                       m_config.log_level);
            return ReplStatus::OK;
        }

        // --- Read-only keys: give a clear error, not a silent no-op ---
        if (key == "sidbase" || key == "runtime" || key == "search-paths" || key == "platform" ||
            key == "mode" || key == "user-profile") {
            fmt::print(fg(fmt::color::yellow),
                       "; ERROR: \"{}\" is read-only (set it on the command line)\n", key);
            return ReplStatus::ERR;
        }

        // --- Unknown key ---
        fmt::print(fg(fmt::color::crimson),
                   "; ERROR: unknown key \"{}\". Try one of: "
                   "print-listing, print-ir, print-ast, print-asm, log-level\n",
                   key);
        return ReplStatus::ERR;
    }

    // (arg-get "key")
    //
    // Read a compiler runtime parameter. Values are read directly
    // from m_config, which is the single source of truth for every
    // compiler option — the same struct the CLI populates at startup.
    //
    // Unknown keys return (unset) rather than an error, so that a
    // script can safely probe for optional keys.
    //
    // Examples:
    //   (arg-get "print-listing")
    //   (arg-get "log-level")
    //   (arg-get "sidbase")
    ReplStatus Compiler::cmd_arg_get(const script::Object &form) {
        // --- Parse the key ---
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: (arg-get \"key\") requires a key\n");
            return ReplStatus::ERR;
        }
        const auto &key_obj = rest.as_pair()->car;
        if (!key_obj.is_string() && !key_obj.is_symbol()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (arg-get ...) key must be string or symbol\n");
            return ReplStatus::ERR;
        }

        const std::string key = key_obj.to_std_string();

        // --- Mutable keys ---
        if (key == "print-listing") {
            fmt::print(fg(fmt::color::green), "; arg-get \"print-listing\" => {}\n",
                       m_config.print_listing ? "#t" : "#f");
            return ReplStatus::OK;
        }
        if (key == "print-ir") {
            fmt::print(fg(fmt::color::green), "; arg-get \"print-ir\" => {}\n",
                       m_config.debug_print_ir ? "#t" : "#f");
            return ReplStatus::OK;
        }
        if (key == "print-ast") {
            fmt::print(fg(fmt::color::green), "; arg-get \"print-ast\" => {}\n",
                       m_config.debug_print_ast ? "#t" : "#f");
            return ReplStatus::OK;
        }
        if (key == "print-asm") {
            fmt::print(fg(fmt::color::green), "; arg-get \"print-asm\" => {}\n",
                       m_config.debug_print_asm ? "#t" : "#f");
            return ReplStatus::OK;
        }
        if (key == "log-level") {
            fmt::print(fg(fmt::color::green), "; arg-get \"log-level\" => \"{}\"\n",
                       m_config.log_level);
            return ReplStatus::OK;
        }

        // --- Read-only keys ---
        if (key == "sidbase") {
            if (m_config.sidbase_path.empty()) {
                fmt::print(fg(fmt::color::yellow), "; arg-get \"sidbase\" => (unset)\n");
            } else {
                fmt::print(fg(fmt::color::green), "; arg-get \"sidbase\" => \"{}\"\n",
                           m_config.sidbase_path);
            }
            return ReplStatus::OK;
        }
        if (key == "runtime") {
            fmt::print(fg(fmt::color::green), "; arg-get \"runtime\" => \"{}\"\n",
                       m_runtime->name());
            return ReplStatus::OK;
        }
        if (key == "user-profile") {
            fmt::print(fg(fmt::color::green), "; arg-get \"user-profile\" => \"{}\"\n",
                       m_config.user_profile);
            return ReplStatus::OK;
        }

        // --- Unknown ---
        fmt::print(fg(fmt::color::yellow), "; arg-get \"{}\" => (unset)\n", key);
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_arg_list(const script::Object &form) {
        // --- Parse (optional) filter argument ---
        auto        rest = form.as_pair()->cdr;
        std::string filter;
        bool        has_filter = false;
        if (rest.is_pair()) {
            const auto &filter_obj = rest.as_pair()->car;
            if (!filter_obj.is_string() && !filter_obj.is_symbol()) {
                fmt::print(fg(fmt::color::crimson),
                           "; ERROR: (arg-list ...) filter must be string or symbol\n");
                return ReplStatus::ERR;
            }
            filter = filter_obj.to_std_string();
            has_filter = true;
        }

        auto matches = [&](const std::string &key) {
            return !has_filter || key.find(filter) != std::string::npos;
        };

        // --- Mutable keys ---
        if (matches("print-listing") || matches("print-ir") || matches("print-ast") ||
            matches("print-asm") || matches("log-level")) {
            fmt::print(fg(fmt::color::cyan), "; --- Mutable keys ---\n");
        }
        if (matches("print-listing")) {
            fmt::print(fg(fmt::color::green), ";   print-listing => {}\n",
                       m_config.print_listing ? "#t" : "#f");
        }
        if (matches("print-ir")) {
            fmt::print(fg(fmt::color::green), ";   print-ir      => {}\n",
                       m_config.debug_print_ir ? "#t" : "#f");
        }
        if (matches("print-ast")) {
            fmt::print(fg(fmt::color::green), ";   print-ast     => {}\n",
                       m_config.debug_print_ast ? "#t" : "#f");
        }
        if (matches("print-asm")) {
            fmt::print(fg(fmt::color::green), ";   print-asm     => {}\n",
                       m_config.debug_print_asm ? "#t" : "#f");
        }
        if (matches("log-level")) {
            fmt::print(fg(fmt::color::green), ";   log-level     => \"{}\"\n", m_config.log_level);
        }

        // --- Read-only keys ---
        if (matches("sidbase") || matches("runtime") || matches("user-profile")) {
            fmt::print(fg(fmt::color::cyan), "; --- Read-only keys ---\n");
        }
        if (matches("sidbase")) {
            if (m_config.sidbase_path.empty()) {
                fmt::print(fg(fmt::color::yellow), ";   sidbase       => (unset)\n");
            } else {
                fmt::print(fg(fmt::color::green), ";   sidbase       => \"{}\"\n",
                           m_config.sidbase_path);
            }
        }
        if (matches("runtime")) {
            fmt::print(fg(fmt::color::green), ";   runtime       => \"{}\"\n", m_runtime->name());
        }
        if (matches("user-profile")) {
            fmt::print(fg(fmt::color::green), ";   user-profile  => \"{}\"\n",
                       m_config.user_profile);
        }

        return ReplStatus::OK;
    }
    // ===============================================================
    // Garbage collector
    // ===============================================================

    namespace {

        struct GcListEntry {
            sid64       type;
            void       *payload;
            std::size_t size;
        };

        struct GcListContext {
            std::vector<GcListEntry>               *entries;
            std::vector<sid64>                     *order;
            std::unordered_map<sid64, std::size_t> *counts;
        };

        void gc_list_visitor(sid64 type_sid, void *payload, std::size_t size, void *user) {
            auto *ctx = static_cast<GcListContext *>(user);

            if (ctx->counts->find(type_sid) == ctx->counts->end()) {
                (*ctx->counts)[type_sid] = 0;
                ctx->order->push_back(type_sid);
            }
            (*ctx->counts)[type_sid] += 1;

            ctx->entries->push_back({type_sid, payload, size});
        }

    } // namespace

    ReplStatus Compiler::cmd_gc_collect(const script::Object &form) {
        (void)form;

        const auto before = gc::stats().live_objects;
        gc::collect();
        const auto after = gc::stats().live_objects;

        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                   "; gc-collect: {} -> {} live objects\n", before, after);
        return ReplStatus::OK;
    }

    ReplStatus Compiler::cmd_gc_list(const script::Object &form) {
        (void)form;

        const auto s = gc::stats();

        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "=== GC heap ===\n");
        fmt::print("  live objects:  {}\n", s.live_objects);
        fmt::print("  total allocs:  {}\n", s.total_allocated);
        fmt::print("  total frees:   {}\n", s.total_freed);
        fmt::print("  passes:        {}\n", s.total_passes);
        fmt::print("  committed:     {} bytes\n", s.bytes_committed);

        std::vector<GcListEntry>               entries;
        std::vector<sid64>                     order;
        std::unordered_map<sid64, std::size_t> counts;

        GcListContext ctx{&entries, &order, &counts};
        gc::for_each_live(&gc_list_visitor, &ctx);

        if (entries.empty()) {
            fmt::print(fg(fmt::color::yellow), "  (no live objects)\n");
            return ReplStatus::OK;
        }

        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nBy type:\n");

        // Sort by count descending.
        std::sort(order.begin(), order.end(),
                  [&](sid64 a, sid64 b) { return counts[a] > counts[b]; });

        for (sid64 sid : order) {
            const std::string name = carbon::StringId(sid).to_string();
            fmt::print("  {:<30} {}\n", name, counts[sid]);
        }

        // Detail list (first 32).
        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "\nObjects (first 32):\n");
        constexpr std::size_t kMaxShow = 32;
        for (std::size_t i = 0; i < entries.size() && i < kMaxShow; ++i) {
            const auto       &e = entries[i];
            const std::string name = carbon::StringId(e.type).to_string();
            fmt::print("  [{}] {:<24} size={}  ptr={}\n", i, name, e.size, e.payload);
        }
        if (entries.size() > kMaxShow) {
            fmt::print("  ... and {} more\n", entries.size() - kMaxShow);
        }

        return ReplStatus::OK;
    }

    // ===============================================================
    // Type system
    // ===============================================================

    ReplStatus Compiler::cmd_ts_info(const script::Object &form) {
        auto rest = form.as_pair()->cdr;
        if (!rest.is_pair()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (ts-info <type-name>) requires a type name\n");
            return ReplStatus::ERR;
        }

        const auto &name_obj = rest.as_pair()->car;
        if (!name_obj.is_symbol() && !name_obj.is_string()) {
            fmt::print(fg(fmt::color::crimson),
                       "; ERROR: (ts-info ...) name must be symbol or string\n");
            return ReplStatus::ERR;
        }

        const std::string type_name = name_obj.to_std_string();

        Type *type = m_ts.lookup_type_no_throw(type_name);
        if (!type) {
            fmt::print(fg(fmt::color::yellow), "; ts-info: unknown type '{}'\n", type_name);
            return ReplStatus::ERR;
        }

        fmt::print("{}\n", type->inspect());

        // Extras that inspect() does not print:
        fmt::print("  is_reference:           {}\n", type->is_reference());
        fmt::print("  load_size:              {}\n", type->get_load_size());
        fmt::print("  preferred_reg_class:    {}\n",
                   reg_kind_to_string(type->get_preferred_reg_class()));

        const auto &states = type->get_states_declared_for_type();
        if (!states.empty()) {
            fmt::print("  states ({}):\n", states.size());
            for (const auto &[k, v] : states) { fmt::print("    {:<24}  {}\n", k, v.print()); }
        }

        return ReplStatus::OK;
    }

    // ===============================================================
    // AST
    // ===============================================================
    
    ReplStatus Compiler::cmd_ast(const script::Object &form) {
        (void)form;

        fmt::print("=== FileNodes ({}) ===\n", m_file_nodes.size());
        for (auto &[name, fn] : m_file_nodes) {
            fmt::print("--- {} ---\n", name);
            inspect_ast(fn.get());
        }

        fmt::print("=== GlobalNode ({}) ===\n", m_global_env->symbols().size());
        inspect_ast(m_global_env.get());

        return ReplStatus::OK;
    }

    // ===============================================================
    // REPL loop helpers
    // ===============================================================

    void Compiler::save_repl_history() { m_repl->save_history(); }

    void Compiler::print_to_repl(const std::string &str) { m_repl->print_to_repl(str); }

    // The prompt depends on the current mode.
    std::string Compiler::get_prompt() {
        std::string prompt = "";

        switch (m_config.mode) {
        case sootc::CompilerMode::COMPILE_ONLY:
            prompt = fmt::format(fmt::emphasis::bold | fg(fmt::color::cyan), "sc > ");
            break;
        case sootc::CompilerMode::INTERPRET_ONLY:
            prompt = fmt::format(fmt::emphasis::bold | fg(fmt::color::cyan), "si > ");
            break;
        case sootc::CompilerMode::HYBRID:
            prompt = fmt::format(fmt::emphasis::bold | fg(fmt::color::cyan), "sci> ");
            break;
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
        if (m_config.debug_print_ast) { lg::info("=> {}", result.print()); }
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

                if (m_config.print_listing) { print_listing(**result); }

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
            if (m_config.debug_print_ast) { lg::info("=> {}", result.print()); }
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
        if (m_config.debug_print_ast) { lg::info("AST: {}", forms.print()); }

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

    void Compiler::load_user_profile() {
        if (m_config.user_profile == "#f") return;

        REPL::StartupFile startup =
            REPL::load_user_startup_file(m_config.user_profile, SootPlatform::Default);

        for (const auto &cmd : startup.run_before_listen) {
            try {
                auto result = interpret(cmd);
            } catch (const std::exception &e) {
                print_warning(fmt::format("Failed to execute startup command: {}", e.what()));
            }
        }

        lg::info("Loaded user profile: {}", m_config.user_profile);
    }

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

        m_repl->username = m_config.user_profile;
        m_repl->init_settings();
        m_repl->load_history();
    }

    void Compiler::color_binary_file(std::unique_ptr<BinaryFile> & /*binary*/) {
        // Register allocation and optimizations would go here.
        if (m_config.debug_print_asm) {
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

        for (const auto &dir : m_config.search_paths) {
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

    // ===============================================================
    // Printing / saving
    // ===============================================================

    void Compiler::print_listing(const BinaryFile &file) {
        carbon::BinaryFileInspector inspector(const_cast<BinaryFile *>(&file));
        inspector.inspect();
    }

    bool Compiler::save_binary(const BinaryFile &file, const std::filesystem::path &target_dir) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::create_directories(target_dir, ec);
        if (ec) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: cannot create dir {}: {}\n",
                       target_dir.string(), ec.message());
            return false;
        }

        // ---- Strict mode: every :methods declaration must have a body ----
        //
        // A REPL session can build a type and its methods in separate
        // inputs; build_ss_type must not fail on the intermediate states.
        // But once the user asks to save the binary, every declared method
        // that is not a builtin must have a ScriptLambda.
        {
            const DCEntry *table = file.entries();
            const u32      n = file.entry_count();
            for (u32 i = 0; i < n; ++i) {
                if (BinaryFile::entry_kind(table[i]) != BinaryFile::EntryKind::SsType) continue;
                const SsType *st = file.entry_as_ss_type(table[i]);
                if (!st || !st->m_pMethods) continue;

                for (u32 id = 0; id < st->m_numMethods; ++id) {
                    const SsMethod &sm = static_cast<const SsMethod *>(st->m_pMethods)[id];
                    if (sm.m_pLambda != nullptr) continue;
                    if (sm.m_name == 0) continue; // empty slot

                    const std::string type_name = file.resolve_sid(st->m_name);
                    const std::string method_name = file.resolve_sid(sm.m_name);

                    // Skip builtins (methods inherited from basic/object that
                    // have no user implementation in this module). The
                    // m_fullName still carries "<type>-<method>"; if the
                    // method's defining type is not this type, it is builtin.
                    //
                    // For a simple first step, just report every unimplemented
                    // method. If this turns out too noisy for builtins, the
                    // check can be relaxed later.
                    fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                               "; ERROR: method '{}' (id {}) of type '{}' has no defmethod\n",
                               method_name, id, type_name);
                    return false;
                }
            }
        }

        fs::path out = target_dir / (file.m_path.stem().string() + ".bin");
        if (!const_cast<BinaryFile &>(file).save(out)) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: cannot save {}\n", out.string());
            return false;
        }

        fmt::print(fg(fmt::color::green), "; saved {}\n", out.string());
        return true;
    }

    bool Compiler::save_listing(const BinaryFile &file, const std::filesystem::path &target_dir) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::create_directories(target_dir, ec);

        fs::path      out = target_dir / "out.lst";
        std::ofstream ofs(out);
        if (!ofs) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: cannot open {}\n", out.string());
            return false;
        }

        // TODO: redirect BinaryFileInspector output here when its API allows
        // writing to a stream.
        ofs << "; SOOT listing\n";
        ofs << "; (not yet wired to BinaryFile API)\n";
        (void)file;

        fmt::print(fg(fmt::color::green), "; saved {}\n", out.string());
        return true;
    }

    bool Compiler::save_header(const BinaryFile &file, const std::filesystem::path &target_dir) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::create_directories(target_dir, ec);

        // Collect type names that actually have an SsType entry in the
        // binary. This filters out all builtin VM types (structure, basic,
        // symbol, type, string, function, ...) and only emits user types.
        std::vector<std::string> type_names;
        const DCEntry           *table = file.entries();
        const u32                n = file.entry_count();
        if (table) {
            for (u32 i = 0; i < n; ++i) {
                if (BinaryFile::entry_kind(table[i]) != BinaryFile::EntryKind::SsType) { continue; }
                const SsType *st = file.entry_as_ss_type(table[i]);
                if (!st) continue;
                const std::string name = file.resolve_sid(st->m_name);
                if (!name.empty()) { type_names.push_back(name); }
            }
        }

        if (type_names.empty()) {
            return true; // nothing to emit
        }

        std::string header = m_ts.generate_c_header(type_names);

        fs::path      out = target_dir / (file.m_path.stem().string() + ".h");
        std::ofstream ofs(out);
        if (!ofs) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: cannot open {} for writing\n",
                       out.string());
            return false;
        }
        ofs << header;

        fmt::print(fg(fmt::color::green), "; saved {}\n", out.string());
        return true;
    }
    bool Compiler::save_session(const std::string &name, const std::filesystem::path &dir) {
        namespace fs = std::filesystem;

        // Resolve the output directory against the project root.
        fs::path out_dir = dir;
        if (out_dir.is_relative()) {
            out_dir = file_util::get_path(file_util::PathType::PROJECT) / out_dir;
        }

        std::error_code ec;
        fs::create_directories(out_dir, ec);
        if (ec) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: cannot create dir {}: {}\n",
                       out_dir.string(), ec.message());
            return false;
        }

        if (m_session_forms.empty()) {
            fmt::print(fg(fmt::color::yellow), "; WARN: session is empty, nothing to save\n");
            return false;
        }

        // save_session builds a throwaway FileNode from m_session_forms.
        // NodeBuilder::build_define binds every (define ...) into
        // m_global_env; without a rollback, those bindings point at
        // Nodes owned by the throwaway FileNode and dangle as soon as
        // it goes out of scope. Snapshot m_global_env before building
        // and roll it back on every exit path.
        GlobalNode::Snapshot global_snapshot = m_global_env->take_snapshot();

        try {
            NodeBuilder builder(m_ts, this);
            auto        file_node = std::make_unique<FileNode>(name);

            auto top_level_body = std::make_unique<SequenceNode>();
            auto top_level = std::make_unique<FunctionNode>("top-level");
            bool top_level_used = false;
            
            fmt::print(stderr, "; save_session: {} forms\n", m_session_forms.size());
            for (auto &f : m_session_forms) { fmt::print(stderr, ";   form: {}\n", f.print()); }

            for (auto &form : m_session_forms) {
                auto node = builder.build(form, file_node.get());

                if (!node) continue;

                if (auto *sd = dynamic_cast<StaticDefineNode *>(node.get())) {
                    file_node->add_child(sd->take_decl());
                    for (auto &store : sd->take_stores()) {
                        top_level_body->add(std::move(store));
                        top_level_used = true;
                    }
                } else if (auto *hd = dynamic_cast<HeapDefineNode *>(node.get())) {
                    file_node->add_child(hd->take_decl());
                    top_level_body->add(hd->take_store());
                    top_level_used = true;
                } else if (dynamic_cast<FunctionNode *>(node.get()) != nullptr ||
                           dynamic_cast<TypeDeclarationNode *>(node.get()) != nullptr ||
                           dynamic_cast<EnumDeclarationNode *>(node.get()) != nullptr ||
                           dynamic_cast<DataDeclarationNode *>(node.get()) != nullptr ||
                           dynamic_cast<StateScriptNode *>(node.get()) != nullptr) {
                    file_node->add_child(std::move(node));
                } else if (auto *expr = dynamic_cast<ExpressionNode *>(node.get())) {
                    if (auto *seq = dynamic_cast<SequenceNode *>(expr); seq && seq->size() == 0) {
                        // Empty sequence at top level: no code, drop it.
                    } else {
                        node.release();
                        top_level_body->add(std::unique_ptr<ExpressionNode>(expr));
                        top_level_used = true;
                    }
                }
            }

            if (top_level_used) {
                top_level->set_body(std::move(top_level_body));
                file_node->add_child(std::move(top_level));
            }

            // Compile state-script handler bodies.
            {
                std::vector<StateScriptNode *> state_scripts;
                for (auto &child : file_node->children()) {
                    if (auto *ss = dynamic_cast<StateScriptNode *>(child.get())) {
                        state_scripts.push_back(ss);
                    }
                }
                for (auto *ss : state_scripts) {
                    compile_state_script_bodies(ss, file_node.get(), builder);
                }
            }

            // ---- Strict mode: every method declared in a type must be
            //      implemented by a defmethod in the same FileNode. ----
            {
                std::set<std::string> fn_names;
                for (auto &child : file_node->children()) {
                    if (auto *fn = dynamic_cast<FunctionNode *>(child.get())) {
                        fn_names.insert(fn->name());
                    }
                }

                for (auto &child : file_node->children()) {
                    auto *td = dynamic_cast<TypeDeclarationNode *>(child.get());
                    if (!td) continue;

                    const std::string &type_name = td->type().base_type();
                    Type              *type = m_ts.lookup_type_no_throw(type_name);
                    if (!type) continue;

                    for (const auto &m : type->get_methods_defined_for_type()) {
                        const std::string full_name = type_name + "-" + m.name;
                        if (fn_names.count(full_name)) continue;

                        fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                                   "; ERROR: method '{}' of type '{}' is declared "
                                   "but has no defmethod\n",
                                   m.name, type_name);
                        m_global_env->rollback(std::move(global_snapshot));
                        return false;
                    }

                    if (const auto *ni = type->get_new_method_defined_for_type()) {
                        const std::string full_name = type_name + "-" + ni->name;
                        if (!fn_names.count(full_name)) {
                            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                                       "; ERROR: method 'new' of type '{}' is declared "
                                       "but has no defmethod\n",
                                       type_name);
                            m_global_env->rollback(std::move(global_snapshot));
                            return false;
                        }
                    }
                }
            }
            // ---- Strict mode: every method declared in a type must be
            //      implemented by a defmethod in the same FileNode. ----
            {
                std::set<std::string> fn_names;
                for (auto &child : file_node->children()) {
                    if (auto *fn = dynamic_cast<FunctionNode *>(child.get())) {
                        fn_names.insert(fn->name());
                    }
                }

                for (auto &child : file_node->children()) {
                    auto *td = dynamic_cast<TypeDeclarationNode *>(child.get());
                    if (!td) continue;

                    const std::string &type_name = td->type().base_type();
                    Type              *type = m_ts.lookup_type_no_throw(type_name);
                    if (!type) continue;

                    for (const auto &m : type->get_methods_defined_for_type()) {
                        const std::string full_name = type_name + "-" + m.name;
                        if (fn_names.count(full_name)) continue;

                        fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                                   "; ERROR: method '{}' of type '{}' is declared "
                                   "but has no defmethod\n",
                                   m.name, type_name);
                        m_global_env->rollback(std::move(global_snapshot));
                        return false;
                    }

                    if (const auto *ni = type->get_new_method_defined_for_type()) {
                        const std::string full_name = type_name + "-" + ni->name;
                        if (!fn_names.count(full_name)) {
                            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                                       "; ERROR: method 'new' of type '{}' is declared "
                                       "but has no defmethod\n",
                                       type_name);
                            m_global_env->rollback(std::move(global_snapshot));
                            return false;
                        }
                    }
                }
            }

            // ============================================================
            // Strip self-dependency.
            //
            // build_require already skips a require whose stem matches
            // file->name(). But the session's identity is the `name`
            // argument of save_session, which may differ from any
            // FileNode::name() seen during the REPL (those are
            // "<repl-N>"). So the user can type (require "bar.sc") in a
            // session they then save as "bar", and the resulting
            // bar.bin would require "bar" — a self-cycle that cannot
            // be loaded. Remove it defensively here.
            //
            // This is a no-op in the normal case, where the user saves
            // the session under a name they never required.
            // ============================================================
            {
                namespace fs = std::filesystem;
                const std::string self = fs::path(name).stem().string();
                if (!self.empty()) {
                    const auto &reqs = file_node->get_requires();
                    if (std::find(reqs.begin(), reqs.end(), self) != reqs.end()) {
                        file_node->remove_require(self);
                    }
                }
            }

            DataSegment strings_table;
            auto        element = file_node->generate(strings_table);
            if (element.m_rawData.empty()) {
                fmt::print(fg(fmt::color::yellow), "; WARN: session produced no binary output\n");
                m_global_env->rollback(std::move(global_snapshot));
                return false;
            }

            auto bytes = make_aligned_buffer(element.m_rawData.size());
            std::memcpy(bytes.get(), element.m_rawData.data(), element.m_rawData.size());

            auto binary_result =
                BinaryFile::from_buffer(name, std::move(bytes), element.m_rawData.size());
            if (!binary_result) {
                fmt::print(fg(fmt::color::crimson), "; ERROR: {}\n", binary_result.error());
                m_global_env->rollback(std::move(global_snapshot));
                return false;
            }

            auto binary = std::make_unique<BinaryFile>(std::move(binary_result.value()));
            for (const auto &ds : file_node->data_structs()) {
                binary->m_dataStructs.push_back(ds);
            }

            if (m_config.print_listing) { print_listing(*binary); }

            bool ok = save_binary(*binary, out_dir);
            if (ok) ok = save_header(*binary, out_dir);

            save_sidbase();
            m_global_env->rollback(std::move(global_snapshot));
            return ok;
        } catch (const std::exception &e) {
            m_global_env->rollback(std::move(global_snapshot));
            fmt::print(fg(fmt::color::crimson), "; EXCEPTION during save_session: {}\n", e.what());
            return false;
        }
    }
    bool Compiler::save_module(const std::filesystem::path &source_file,
                               const std::filesystem::path &out_dir) {
        namespace fs = std::filesystem;

        // 1. Resolve the source path against search_paths.
        const std::string resolved = find_file(source_file.string());
        if (!fs::exists(resolved)) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                       "; ERROR: save_module: '{}' not found\n", source_file.string());
            return false;
        }

        // 2. Compile the file. This is the SAME path as a normal `sootc
        //    file.sc` run: it builds a fresh FileNode named after the file
        //    stem, so its m_requires are exactly the file's own requires.
        lg::info("save_module: compiling '{}'", resolved);

        auto result = compile_file(fs::path(resolved));
        if (!result) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                       "; ERROR: save_module: compile failed: {}\n", result.error());
            return false;
        }

        // Compile-time-only file (only deftype/defenum/defconstant): nothing
        // to save. That is a normal outcome; report and return true.
        if (!*result) {
            fmt::print(fg(fmt::color::yellow),
                       "; WARN: save_module: '{}' produced no binary (declarations only)\n",
                       source_file.string());
            return true;
        }

        // 3. Strip self-dependency defensively.
        const std::string stem = fs::path(resolved).stem().string();
        if (auto *file_node = find_file_node(resolved)) { file_node->remove_require(stem); }

        // 4. Resolve the output directory against PROJECT.
        fs::path target = out_dir;
        if (target.is_relative()) {
            target = file_util::get_path(file_util::PathType::PROJECT) / target;
        }

        // 5. Save. save_binary enforces the "every :methods entry must have
        //    a body" rule and writes <stem>.bin; save_header writes
        //    <stem>.h from the SsType entries.
        if (!save_binary(**result, target)) { return false; }
        if (!save_header(**result, target)) { return false; }

        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                   "; saved module '{}' -> {}/{}.bin\n", stem, target.string(), stem);
        return true;
    }
    void Compiler::save_sidbase() {
        if (m_config.sidbase_path.empty()) {
            return; // no path configured, nothing to save
        }

        const std::size_t count = carbon::StringIdManager::instance().size();
        const bool        saved =
            carbon::StringIdManager::instance().save_dconstruct_sidbase(m_config.sidbase_path);

        if (saved) {
            lg::info("SIDBase: saved {} entries to '{}'", count, m_config.sidbase_path);
        } else {
            lg::warn("SIDBase: failed to save '{}'", m_config.sidbase_path);
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

    // ========================================================================
    // State-script body compilation
    // ========================================================================

    namespace {

        // Build a (lambda () body) form from an arbitrary body form.
        // If the form is already a lambda, use it as-is.
        script::Object ensure_lambda(script::Interpreter &soot, const script::Object &body) {
            if (body.is_pair() && body.as_pair()->car.is_symbol()) {
                const std::string head = body.as_pair()->car.as_symbol();
                if (head == "lambda" || head == "function") { return body; }
            }
            // Wrap: (lambda () body)
            return script::Object::make_pair(
                soot.intern("lambda"),
                script::Object::make_pair(
                    script::Object::make_null(),
                    script::Object::make_pair(body, script::Object::make_null())));
        }

        std::string handler_kind_name(StateHandler kind) {
            switch (kind) {
            case StateHandler::ENTER: return "enter";
            case StateHandler::EXIT: return "exit";
            case StateHandler::CODE: return "code";
            case StateHandler::UPDATE: return "trans";
            case StateHandler::POST: return "post";
            case StateHandler::EVENT: return "event";
            }
            return "unknown";
        }

    }
    
    // namespace
    // (lambda () form...) from a list of forms: implicit begin, ONE lambda.
    script::Object lambda_from_forms(script::Interpreter               &soot,
                                     const std::vector<script::Object> &forms) {
        return script::Object::make_pair(
            soot.intern("lambda"),
            script::Object::make_pair(script::Object::make_null(), script::build_list(forms)));
    }

    void Compiler::compile_state_script_bodies(StateScriptNode *ss, FileNode *file,
                                               NodeBuilder &builder) {
        const std::string &type_name = ss->type().base_type();

        Type *t = m_ts.lookup_type_no_throw(type_name);
        if (!t) {
            lg::warn("compile_state_script_bodies: unknown type '{}'", type_name);
            return;
        }

        // Compile one lambda form into a named FunctionNode owned by the file.
        auto emit = [&](const std::string &name, const script::Object &lambda_form,
                        const script::Object &where, const std::string &what) {
            try {
                auto  node = builder.build_lambda(lambda_form, file);
                auto *fn = dynamic_cast<FunctionNode *>(node.get());
                if (!fn) {
                    lg::warn("compile_state_script_bodies: '{}' did not produce a FunctionNode",
                             name);
                    return;
                }
                fn->set_name(name);
                FunctionNode *raw = fn;
                node.release();
                file->add_child(std::unique_ptr<Node>(raw));
                file->bind(name, raw);
            } catch (const std::exception &e) {
                throw make_error(where, fmt::format("compiling {}", what))
                    .expected("a compilable lambda body")
                    .got(e.what());
            }
        };

        for (const auto &[state_name, def] : t->state_defs()) {
            // ---- Handlers ----
            for (const auto &h : def.handlers) {
                if (h.body_ast.is_null()) continue;

                const std::string base =
                    fmt::format("{}-{}-{}", type_name, state_name, handler_kind_name(h.kind));
                const std::string name = (h.kind == StateHandler::EVENT)
                                             ? fmt::format("{}-event-{}", base, h.event_id)
                                             : base;

                emit(name, ensure_lambda(m_soot, h.body_ast), h.body_ast,
                     fmt::format("handler '{}'", name));
            }

            // ---- Tracks (belong to the state) ----
            for (const auto &tr : def.tracks) {
                const std::string prefix =
                    fmt::format("{}-{}-track-{}", type_name, state_name, tr.name);

                // On-begin coroutine: ONE lambda for the whole body.
                if (!tr.body.empty()) {
                    emit(prefix + "-main", lambda_from_forms(m_soot, tr.body), tr.body.front(),
                         fmt::format("track '{}' body", tr.name));
                }

                // Reactive frame triggers: one lambda each, named by index.
                for (size_t i = 0; i < tr.frames.size(); ++i) {
                    const auto &ft = tr.frames[i];
                    emit(fmt::format("{}-frame-{}", prefix, i), lambda_from_forms(m_soot, ft.body),
                         ft.body.front(), fmt::format("track '{}' frame {}", tr.name, ft.frame));
                }
            }
        }

        lg::debug("compile_state_script_bodies: '{}' compiled", type_name);
    }

    // ===============================================================
    // Prelude loading
    // ===============================================================

    void Compiler::load_soot_prelude() {
        namespace fs = std::filesystem;

        std::vector<fs::path> candidates = {
            file_util::get_path(file_util::PathType::PROJECT) / "soot_src" / "lib.st",
            file_util::get_path(file_util::PathType::CONFIG) / "soot_src" / "lib.st",
            file_util::get_path(file_util::PathType::SHARE) / "soot_src" / "lib.st",
        };

        for (const auto &p : candidates) {
            if (!fs::exists(p)) { continue; }

            lg::info("Loading SOOT library {}", p.string());

            try {
                std::string content = file_util::read_text(p);

                // Strip UTF-8 BOM.
                if (content.size() >= 3 && static_cast<uint8_t>(content[0]) == 0xEF &&
                    static_cast<uint8_t>(content[1]) == 0xBB &&
                    static_cast<uint8_t>(content[2]) == 0xBF) {
                    content = content.substr(3);
                }

                m_soot.eval_string(content, p.string());

                lg::info("Loaded SOOT library {}", p.string());
                return;

            } catch (const std::exception &e) {
                lg::error("Failed to evaluate {}: {}", p.string(), e.what());
                return;
            }
        }

        lg::warn("lib.st not found");
    }

    void Compiler::load_soc_prelude() {
        namespace fs = std::filesystem;

        std::vector<fs::path> candidates = {
            file_util::get_path(file_util::PathType::PROJECT) / "soot_src" / "lib.sc",
            file_util::get_path(file_util::PathType::CONFIG) / "soot_src" / "lib.sc",
            file_util::get_path(file_util::PathType::SHARE) / "soot_src" / "lib.sc",
        };

        for (const auto &p : candidates) {
            if (!fs::exists(p)) { continue; }

            lg::info("Loading SOOTC library {}", p.string());

            try {
                auto result = compile_file(p);

                if (!result) {
                    lg::error("Failed to compile {}: {}", p.string(), result.error());
                    return;
                }

                if (*result) { m_runtime->load_module(std::move(**result)); }

                lg::info("Loaded SOOTC library {}", p.string());
                return;

            } catch (const std::exception &e) {
                lg::error("Exception while loading {}: {}", p.string(), e.what());
                return;
            }
        }

        lg::warn("lib.sc not found");
    }

    // sootccompilerCompiler.cpp
    void Compiler::sync_type_verbose() {
        const bool verbose = (m_config.log_level == "debug");
        if (Type::verbose != verbose) {
            Type::verbose = verbose;
            lg::debug("Type::verbose = {}", verbose ? "true" : "false");
        }
    }

    bool Compiler::check_type_methods_implemented(const FileNode *file_node) const {
        if (!file_node) return true;

        std::set<std::string> fn_names;
        for (auto &child : file_node->children()) {
            if (auto *fn = dynamic_cast<FunctionNode *>(child.get())) {
                fn_names.insert(fn->name());
            }
        }

        for (auto &child : file_node->children()) {
            auto *td = dynamic_cast<TypeDeclarationNode *>(child.get());
            if (!td) continue;

            const std::string &type_name = td->type().base_type();
            Type              *type = m_ts.lookup_type_no_throw(type_name);
            if (!type) continue;

            for (const auto &m : type->get_methods_defined_for_type()) {
                const std::string full_name = type_name + "-" + m.name;
                if (fn_names.count(full_name)) continue;
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                           "; ERROR: method '{}' of type '{}' is declared "
                           "but has no defmethod\n",
                           m.name, type_name);
                return false;
            }

            if (const auto *ni = type->get_new_method_defined_for_type()) {
                const std::string full_name = type_name + "-" + ni->name;
                if (!fn_names.count(full_name)) {
                    fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                               "; ERROR: method 'new' of type '{}' is declared "
                               "but has no defmethod\n",
                               type_name);
                    return false;
                }
            }
        }
        return true;
    }
} // namespace sootc