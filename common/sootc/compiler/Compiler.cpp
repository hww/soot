// sootc/compiler/Compiler.cpp
#include "fmt/color.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include "third_party/replxx/include/replxx.hxx"
#include "carbon/file/DCIFile.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/file/BinaryFileInspector.hpp"
#include "carbon/file/SizeAssertions.hpp"
#include "carbon/vm/VirtualMachine.hpp"
#include "carbon/file/SizeAssertions.hpp"
#include "carbon/lib/StringIdManager.hpp"
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
#include "sootc/node/SequenceNode.hpp"
#include "sootc/node/TypeDeclarationNode.hpp"
#include "sootc/runtime/LocalVmRuntime.hpp"
#include "sootc/runtime/NullRuntime.hpp"
#include "sootc/runtime/RemoteRuntime.hpp"
#include "util/FileUtil.hpp"
#include "util/Log.hpp"
#include "type_system/TypeSystem.hpp"

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
        m_config = comp_options;

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
            m_runtime = std::make_unique<LocalVmRuntime>();
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
                                "(save-session "};

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
        std::string     err;
        if (!script::get_va(rest, &err, &args)) { throw_compiler_error(form, "{}", err); }
        return args;
    }

    // Same as get_va, but does not handle named arguments.
    script::Arguments Compiler::get_va_no_named(const script::Object &form, const script::Object &rest) {
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
        if (!script::va_check(args, unnamed, named, &err)) { throw_compiler_error(form, "{}", err); }
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

    // Compile a file by path. Reads the source, parses it, then delegates
    // to the Object-based overload.
    std::expected<std::unique_ptr<BinaryFile>, std::string>
    Compiler::compile_file(const std::filesystem::path &path) {
        std::string content = read_file_content(path.string());
        auto        forms = m_soot.get_reader().read_from_string(content, false, path.string());
        if (forms.is_null()) {
            return std::unexpected("Failed to read or parse file: " + path.string());
        }
        return compile_file(forms, path.string());
    }

    // Compile already-parsed forms. Optionally prints IR if configured.
    std::expected<std::unique_ptr<BinaryFile>, std::string>
    Compiler::compile_file(script::Object &forms, const std::string &filename) {
        m_current_file = filename;
        auto result = compile_internal(forms, filename);

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
    // The file is resolved against the compiler's search_paths. If the
    // resolved path has already been loaded (in this Compiler instance,
    // via an earlier require or a user action), the call is a no-op.
    // Otherwise the file is compiled and the resulting module is
    // registered in the active runtime.
    //
    // See the header comment on Compiler::require_file for the
    // rationale, including the handling of cyclic requires.
    bool Compiler::require_file(const script::Object &form, const std::string &path) {
        // ------------------------------------------------------------------
        // 1. Resolve the path against search_paths.
        // ------------------------------------------------------------------
        namespace fs = std::filesystem;

        std::string resolved = path;

        if (!fs::exists(resolved)) {
            bool found = false;
            for (const auto &dir : m_config.search_paths) {
                fs::path candidate = fs::path(dir) / path;
                if (fs::exists(candidate)) {
                    resolved = candidate.string();
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw CompilerError("Compiler::require_file")
                    .expected("a file that exists in search_paths")
                    .got(fmt::format("'{}'", path))
                    .note(fmt::format("search_paths: {}", fmt::join(m_config.search_paths, ", ")));
            }
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
    std::expected<std::unique_ptr<BinaryFile>, std::string>
    Compiler::compile_internal(script::Object &forms, const std::string &filename) {
        try {
            NodeBuilder builder(m_ts, this);

            // ------------------------------------------------------------------
            // Reuse the existing FileNode for this name if one is already
            // registered.
            //
            // For the REPL, filename is always "<repl>", so this makes a
            // `define` from an earlier input visible to later inputs:
            // build_variable can look up the name in the same FileNode.
            //
            // For non-REPL names (real files), the map does not contain the
            // name on the first (and only) compile, so behaviour is
            // unchanged.
            // ------------------------------------------------------------------
            std::unique_ptr<FileNode> file_node_owned;
            FileNode                 *file_node = nullptr;

            {
                auto it = m_file_nodes.find(filename);
                if (it != m_file_nodes.end()) {
                    file_node = it->second.get();
                } else {
                    file_node_owned = std::make_unique<FileNode>(filename);
                    file_node = file_node_owned.get();
                }
            }

            // Top-level is a sequence of forms. Functions and declarations
            // become children of the FileNode; everything else goes into
            // a synthetic "top-level" function body.
            auto top_level_body = std::make_unique<SequenceNode>();
            auto top_level = std::make_unique<FunctionNode>("top-level");
            bool top_level_used = false;

            auto current = forms;
            while (current.is_pair()) {
                auto node = builder.build(current.as_pair()->car, file_node);
                if (node) {
                    if (dynamic_cast<FunctionNode *>(node.get()) != nullptr) {
                        // Top-level function: separate child of FileNode.
                        file_node->add_child(std::move(node));
                    } else if (dynamic_cast<TypeDeclarationNode *>(node.get()) != nullptr ||
                               dynamic_cast<EnumDeclarationNode *>(node.get()) != nullptr) {
                        // Type/enum declaration: compile-time only, no code.
                        file_node->add_child(std::move(node));
                    } else if (dynamic_cast<DataDeclarationNode *>(node.get()) != nullptr) {
                        // Top-level data declaration: (define name (new Type ...)).
                        file_node->add_child(std::move(node));
                    } else if (dynamic_cast<NewNode *>(node.get()) != nullptr) {
                        // A bare (new ...) at top level is not allowed;
                        // it must be wrapped in (define ...).
                        throw CompilerError("Compiler::compile_internal")
                            .where(fmt::format("file '{}'", filename))
                            .expected("top-level 'new' to be wrapped in 'define'")
                            .got("bare (new ...)");
                    } else if (auto *expr = dynamic_cast<ExpressionNode *>(node.get())) {
                        // Regular expression: goes into the top-level body.
                        node.release();
                        top_level_body->add(std::unique_ptr<ExpressionNode>(expr));
                        top_level_used = true;
                    } else {
                        throw CompilerError("Compiler::compile_internal")
                            .where(fmt::format("file '{}'", filename))
                            .expected("top-level form to be FunctionNode, TypeDeclarationNode, "
                                      "EnumDeclarationNode, or ExpressionNode")
                            .got(node->get_node_type_string());
                    }
                }
                current = current.as_pair()->cdr;
            }

            if (top_level_used) {
                top_level->set_body(std::move(top_level_body));
                file_node->add_child(std::move(top_level));
            }

            // ------------------------------------------------------------------
            // Persist the FileNode if it was newly created in this call.
            //
            // For repeated names (REPL "<repl>"), the existing FileNode was
            // reused from the start; its children were added directly above
            // and it is already stored in m_file_nodes.
            // ------------------------------------------------------------------
            FileNode *stored = nullptr;
            if (file_node_owned) {
                m_file_nodes[filename] = std::move(file_node_owned);
                stored = m_file_nodes[filename].get();
            } else {
                stored = file_node;
            }

            StringsTable strings_table;
            auto         element = stored->generate(strings_table);

            // Nothing to emit: the file contained only compile-time
            // declarations (deftype / defenum). The types are already
            // registered in the TypeSystem, so there is nothing to write.
            if (element.m_rawData.empty()) {
                lg::info("No binary output (only type/enum declarations)");
                return std::unique_ptr<BinaryFile>(nullptr);
            }

            if (m_config.debug_print_ir) { element.dump(); }

            auto bytes = make_aligned_buffer(element.m_rawData.size());
            std::memcpy(bytes.get(), element.m_rawData.data(), element.m_rawData.size());

            auto binary_result =
                BinaryFile::from_buffer(filename, std::move(bytes), element.m_rawData.size());
            if (!binary_result) {
                return std::unexpected("Failed to create binary from buffer: " +
                                       binary_result.error());
            }

            auto binary = std::make_unique<BinaryFile>(std::move(binary_result.value()));

            // Copy data-struct layouts collected by FileNode into the BinaryFile
            // so that BinaryFileInspector can decode payloads without TypeSystem.
            for (const auto &ds : stored->data_structs()) { binary->m_dataStructs.push_back(ds); }

            if (m_config.debug_print_asm) { color_binary_file(binary); }

            return binary;

        } catch (const script::EvalException &e) {
            fmt::print(fg(fmt::color::orange), "SOOT error while compiling {}: {}\n", filename,
                       e.what());
            return std::unexpected(std::string("SOOT error: ") + e.what());
        } catch (const CompilerError &e) {
            e.render();
            return std::unexpected(std::string(e.what()));
        } catch (const std::exception &e) {
            render_internal_error(e);
            return std::unexpected(std::string("Internal compiler error: ") + e.what());
        }
    }

    // Render a C++ exception that escaped NodeBuilder/Compiler internals.
    // This is a bug in the compiler, not in user code.
    void Compiler::render_internal_error(const std::exception &e) {
        fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                   "\n╔══════════════════════════════════════════════════╗\n");
        fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                   "║       INTERNAL COMPILER ERROR (BUG)              ║\n");
        fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                   "╚══════════════════════════════════════════════════╝\n\n");
        fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold,
                   "This is a bug in the compiler, not in your code.\n\n");
        fmt::print(fg(fmt::color::dim_gray), "Exception: {}\n", e.what());
        fmt::print(fg(fmt::color::dim_gray), "Type:      {}\n", typeid(e).name());
        fmt::print("\n");
        fmt::print(fg(fmt::color::dim_gray),
                   "Please report this with the .sc file that triggered it.\n\n");
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
        if (first == std::string::npos) return ReplStatus::OK; // all whitespace

        std::string trimmed = input.substr(first);

        m_want_exit = false;
        m_want_reload = false;

        // REPL commands are handled BEFORE compilation.
        if (auto status = try_handle_repl_command(trimmed)) {
            if (*status == ReplStatus::WANT_EXIT) return ReplStatus::WANT_EXIT;
            if (*status == ReplStatus::WANT_RELOAD) return ReplStatus::WANT_RELOAD;
            return *status;
        }

        switch (m_config.mode) {
        case CompilerMode::INTERPRET_ONLY: interpret_and_print(trimmed); break;
        case CompilerMode::COMPILE_ONLY:
        case CompilerMode::HYBRID:
        default: compile_and_report(trimmed); break;
        }

        if (m_want_exit) return ReplStatus::WANT_EXIT;
        if (m_want_reload) return ReplStatus::WANT_RELOAD;
        return ReplStatus::OK;
    }

    // The set of names recognised as REPL commands. Kept as a
    // function-local static so it is built exactly once per process.
    const std::set<std::string> &Compiler::repl_commands() {
        static const std::set<std::string> kCommands = {
            "help",
            "clear",
            "e",
            "reload",
            "sizes",
            "inspect-env",
            "inspect-env-soots",
            "has-macro",
            "list-symbols",
            "save-session",
            "sc-run",
            "sc-eval",
            "arg-set",
            "arg-get",
            "arg-ls",
            "gcls",
            "gc",
        };
        return kCommands;
    }

    // If `input` parses to a REPL command form, dispatch it and return
    // its status. Otherwise return std::nullopt so the caller can fall
    // through to the normal compile pipeline.
    std::optional<ReplStatus> Compiler::try_handle_repl_command(const std::string &input) {
        // Legacy `:command` syntax is no longer supported. Detect it
        // early and give the user a helpful hint instead of a raw
        // "undefined variable ':run'" from the compiler.
        if (!input.empty() && input[0] == ':') {
            fmt::print(fg(fmt::color::yellow), "; WARN: ':' command syntax is no longer supported. "
                                               "Use (e), (reload), (help), (sc-run ...), etc.\n");
            return ReplStatus::OK;
        }

        // Quick reject: if it does not start with '(', it cannot be a
        // command form. Anything else goes through the normal pipeline.
        if (input.empty() || input[0] != '(') return std::nullopt;
        // Parse the input. With add_top_level = false, the reader
        // returns a LIST of forms — even for a single-form input.
        auto form = m_soot.get_reader().read_from_string(input, false, "<repl>");
        if (form.is_null() || !form.is_pair()) return std::nullopt;

        // Unwrap the one-element list produced by add_top_level = false.
        //
        // After this step, `form` is the actual user form, e.g.
        // `(has-macro "defun")`. We detect the wrapper by checking
        // whether the head is a pair (which means "this is a list of
        // forms") rather than a symbol.
        if (form.as_pair()->car.is_pair()) { form = form.as_pair()->car; }

        if (!form.is_pair()) return std::nullopt;

        const auto &head = form.as_pair()->car;
        if (!head.is_symbol()) return std::nullopt;

        const std::string name = head.as_symbol().name_ptr;
        if (!repl_commands().count(name)) return std::nullopt;

        return dispatch_repl_command(name, form);
    }

    ReplStatus Compiler::dispatch_repl_command(const std::string &name, const script::Object &form) {
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
        if (name == "sc-run") return cmd_sc_run(form);
        if (name == "sc-eval") return cmd_sc_eval(form);
        if (name == "arg-set") return cmd_arg_set(form);
        if (name == "arg-get") return cmd_arg_get(form);
        if (name == "arg-ls") return cmd_arg_list(form);
        if (name == "gcls") return cmd_gc_list(form);
        if (name == "gc") return cmd_gc_collect(form);

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
    ReplStatus Compiler::compile_and_report(const std::string &code) {
        try {
            auto forms = m_soot.get_reader().read_from_string(code, false, "<repl>");
            if (forms.is_null()) {
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                           "; ERROR: failed to read input\n");
                return ReplStatus::ERR;
            }
            // Remember every top-level form for :save. The forms are stored
            // as parsed script::Object values, which are reference-counted and
            // cheap to keep around.
            {
                auto cur = forms;
                while (cur.is_pair()) {
                    m_session_forms.push_back(cur.as_pair()->car);
                    cur = cur.as_pair()->cdr;
                }
            }

            auto result = compile_file(forms, "<repl>");

            // `result` is std::expected<unique_ptr<BinaryFile>, string>.
            // `!result` is true when the expected holds an error, NOT when
            // the pointer inside is null. We must check both separately.
            if (!result) {
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; ERROR: {}\n",
                           result.error());
                return ReplStatus::ERR;
            }

            // `*result` is the unique_ptr<BinaryFile> itself. It is empty
            // when the input contained only compile-time declarations
            // (deftype, defenum, defmethod, defconstant, ...), which produce
            // no binary payload. That is a normal outcome, not a crash.
            if (!*result) {
                fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                           "; OK (types/enums registered)\n");
                return ReplStatus::OK;
            }

            // print_listing is now the single source of truth: it can
            // be set from the CLI (--print-listing) or from the REPL
            // with (arg-set "print-listing" #t). Both paths write to
            // m_config, so this check has nothing left to look up.
            if (m_config.print_listing) { print_listing(**result); }

            // Hand the compiled module to the active runtime. This is
            // the ONE place where sootc talks to the execution backend
            // during a compile: previously it called
            // carbon::Globals::inst().load_module(...) directly, which
            // hard-wired the in-process VM. Now it goes through the
            // Runtime abstraction, so the same code path works with
            // LocalVmRuntime, RemoteRuntime, and NullRuntime.
            bool loaded = m_runtime->load_module(std::move(**result));
            if (!loaded) {
                fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold,
                           "; WARN: failed to register module in runtime '{}'\n",
                           m_runtime->name());
            }

            // After the module is loaded, run its `top-level` function if
            // it has one. In REPL mode, every expression that is not a
            // declaration (defun / deftype / define with a function value)
            // ends up in a synthetic `top-level` function. Running it here
            // makes `(_print "Hello")` behave like an expression statement:
            // it executes immediately, and its value (if any) is printed.
            //
            // If there is no `top-level` (for example, the input was just
            // a `defun`), the lookup returns nothing and we silently skip.
            if (m_runtime->has_symbol("top-level")) {
                auto top_level_result = m_runtime->call("top-level", {});
                if (top_level_result.ok) {
                    // Print the return value only if it is not the dummy
                    // `1` that _print returns. A more elaborate version
                    // could compare against the function's declared return
                    // type, but for now the check below is enough.
                    const std::string printed = top_level_result.value.to_string();
                    if (printed != "1" && printed != "null" && !printed.empty()) {
                        fmt::print(fg(fmt::color::cyan), "; => {}\n", printed);
                    }
                } else {
                    fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; ERROR: {}\n",
                               top_level_result.error);
                    return ReplStatus::ERR;
                }
            }

            fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "; OK\n");
            return ReplStatus::OK;
        } catch (const std::exception &e) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; EXCEPTION: {}\n",
                       e.what());
            return ReplStatus::ERR;
        }
    }

    // Try to interpret; on failure, fall back to compilation.
    ReplStatus Compiler::try_interpret_then_compile(const std::string &code) {
        try {
            auto result = interpret(code);
            if (m_config.debug_print_ast) { lg::info("=> {}", result.print()); }
            return ReplStatus::OK;
        } catch (const std::exception &e) {
            lg::debug("Interpret failed: {}, trying compilation", e.what());
            return compile_and_report(code);
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

        try {
            NodeBuilder builder(m_ts, this);
            auto        file_node = std::make_unique<FileNode>(name);

            auto top_level_body = std::make_unique<SequenceNode>();
            auto top_level = std::make_unique<FunctionNode>("top-level");
            bool top_level_used = false;

            for (auto &form : m_session_forms) {
                auto node = builder.build(form, file_node.get());
                if (!node) continue;

                if (dynamic_cast<FunctionNode *>(node.get()) != nullptr ||
                    dynamic_cast<TypeDeclarationNode *>(node.get()) != nullptr ||
                    dynamic_cast<EnumDeclarationNode *>(node.get()) != nullptr ||
                    dynamic_cast<DataDeclarationNode *>(node.get()) != nullptr) {
                    file_node->add_child(std::move(node));
                } else if (auto *expr = dynamic_cast<ExpressionNode *>(node.get())) {
                    node.release();
                    top_level_body->add(std::unique_ptr<ExpressionNode>(expr));
                    top_level_used = true;
                }
            }

            if (top_level_used) {
                top_level->set_body(std::move(top_level_body));
                file_node->add_child(std::move(top_level));
            }

            StringsTable strings_table;
            auto         element = file_node->generate(strings_table);
            if (element.m_rawData.empty()) {
                fmt::print(fg(fmt::color::yellow), "; WARN: session produced no binary output\n");
                return false;
            }

            auto bytes = make_aligned_buffer(element.m_rawData.size());
            std::memcpy(bytes.get(), element.m_rawData.data(), element.m_rawData.size());

            auto binary_result =
                BinaryFile::from_buffer(name, std::move(bytes), element.m_rawData.size());
            if (!binary_result) {
                fmt::print(fg(fmt::color::crimson), "; ERROR: {}\n", binary_result.error());
                return false;
            }

            auto binary = std::make_unique<BinaryFile>(std::move(binary_result.value()));
            for (const auto &ds : file_node->data_structs()) {
                binary->m_dataStructs.push_back(ds);
            }

            // Save binary, header, and dci.
            if (!save_binary(*binary, out_dir)) return false;
            if (!save_header(*binary, out_dir)) return false;
            if (!save_dci(*binary, out_dir)) return false;

            return true;
        } catch (const std::exception &e) {
            fmt::print(fg(fmt::color::crimson), "; EXCEPTION during save_session: {}\n", e.what());
            return false;
        }
    }
    // Save a .dci manifest for a compiled module.
    //
    // A .dci file is the module's declaration: its logical path, the
    // imports it depends on, and the exports it provides. The loader
    // (carbon) reads it to resolve dependencies without the user
    // having to list them on the command line.
    //
    // For now, only the imports side is populated. Exports would
    // require tracking every top-level name that ended up in the
    // binary, which the current pipeline does not do. The .dci is
    // still useful: a loader that walks the import graph can order
    // its loads correctly.
    bool Compiler::save_dci(const BinaryFile &file, const std::filesystem::path &target_dir) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::create_directories(target_dir, ec);

        // --- Logical path ---
        //
        // Compute the module's logical path from the file's path,
        // relative to the source_root. Strip the extension and
        // normalise separators to '/'.
        std::string logical_path = file.m_path.string();

        if (!m_config.search_paths.empty()) {
            // Find the first search path that the file lives under,
            // and strip it. Fall back to the file's own stem.
            for (const auto &root : m_config.search_paths) {
                fs::path root_abs = fs::absolute(root, ec);
                fs::path file_abs = fs::absolute(file.m_path, ec);
                if (ec) continue;

                auto rel = fs::relative(file_abs, root_abs, ec);
                if (!ec && !rel.empty() && rel.string().rfind("..", 0) != 0) {
                    logical_path = rel.replace_extension("").generic_string();
                    break;
                }
            }
        }

        if (logical_path.empty()) { logical_path = file.m_path.stem().string(); }

        // --- Build the manifest ---
        carbon::DCIFile manifest;
        manifest.logical_path = logical_path;
        manifest.module_name = carbon::DCIFile::extract_module_name(logical_path);
        manifest.binary_size = static_cast<u32>(file.m_size);

        // Imports: every file that was pulled in via (require ...).
        for (const auto &dep : m_loaded_files) {
            // Store the imported module by its logical path, not by
            // its absolute filesystem path, so the manifest is
            // portable between machines.
            std::string dep_path = dep.string();
            if (!m_config.search_paths.empty()) {
                for (const auto &root : m_config.search_paths) {
                    fs::path root_abs = fs::absolute(root, ec);
                    fs::path dep_abs = fs::absolute(dep, ec);
                    if (ec) continue;

                    auto rel = fs::relative(dep_abs, root_abs, ec);
                    if (!ec && !rel.empty() && rel.string().rfind("..", 0) != 0) {
                        dep_path = rel.replace_extension("").generic_string();
                        break;
                    }
                }
            }
            manifest.imports.emplace_back(carbon::StringId(dep_path));
        }

        // Exports: for now, leave empty. A future change to the
        // compiler can record every top-level name that was emitted
        // and fill this in.
        //
        // manifest.exports = ...;

        // --- Write the .dci ---
        fs::path out = target_dir / (file.m_path.stem().string() + ".dci");
        if (!manifest.save(out.string())) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: cannot write {}\n", out.string());
            return false;
        }

        fmt::print(fg(fmt::color::green), "; saved {}\n", out.string());
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
} // namespace sootc