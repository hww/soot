// sootc/compiler/Compiler.cpp
#include "fmt/color.h"
#include "fmt/core.h"
#include "third_party/replxx/include/replxx.hxx"

#include "carbon/file/BinaryFileInspector.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/vm/VirtualMachine.hpp"
#include "common/soot/Interpreter.hpp"
#include "common/soot/Object.hpp"
#include "common/soot/ParseHelpers.hpp"
#include "common/soot/Reader.hpp"
#include "common/util/Log.hpp"
#include "sootc/compiler/Compiler.hpp"
#include "sootc/compiler/CompilerError.hpp"
#include "sootc/compiler/FileCompiler.hpp"
#include "sootc/compiler/NodeBuilder.hpp"
#include "sootc/node/FileNode.hpp"
#include "sootc/node/SequenceNode.hpp"
#include "type_system/TypeSystem.hpp"
#include "util/FileUtil.hpp"
#include "util/Log.hpp"
#include "file/SizeAssertions.hpp"
#include "sootc/node/DataDeclarationNode.hpp"
#include "sootc/node/EnumDeclarationNode.hpp"
#include "sootc/node/TypeDeclarationNode.hpp"
#include "sootc/compiler/Macro.hpp"

namespace sootc {

    // Construct the compiler and bring it to a usable state:
    //   1. load the SOOT interpreter prelude (lib.sot)  -> m_soot
    //   2. compile the SOOTC compiler prelude (lib.soc) -> m_ts / m_soot macros
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

        // 1. SOOT prelude -> interpreter (lib.sot).
        load_soot_prelude();

        // 2. SOOTC prelude -> compiler (lib.soc).
        load_soc_prelude();

        if (m_config.mode != CompilerMode::COMPILE_ONLY) { setup_repl(); }

        if (user_profile != "#f") { load_user_profile(); }

        // REPL wiring (history, welcome, callbacks).
        if (m_repl) {
            m_repl->load_history();

            m_repl->examples = {"(define ", "(defun ",  "(defmacro ",  "(if ",      "(when ",
                                "(unless ", "(cond ",   "(let ",       "(lambda ",  "(set! ",
                                "(quote ",  "(load \"", "(compile \"", "(deftype ", "(defenum "};

            m_repl->regex_colors = {
                {";[^\n]*", replxx::Replxx::Color::BRIGHTCYAN},
                {"\"[^\"]*\"", replxx::Replxx::Color::GREEN},
                {"\\b[0-9]+\\b", replxx::Replxx::Color::YELLOW},
                {"\\b(defun|defmacro|if|when|unless|cond|let|lambda|set!|define|quote|deftype|"
                 "defenum)\\b",
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

    Compiler::~Compiler() = default;

    // Register C++-side forms that need access to the compiler's state.
    // Only get-enum-vals lives here; everything else is defined in lib.soc.
    void Compiler::setup_soot_forms() {
        // Argument spec: one positional argument (the enum name).
        soot::ArgumentSpec spec(true, false);

        m_soot.add_custom_form(
            "get-enum-vals",
            [this](const soot::Object &form, soot::Arguments &args,
                   const std::shared_ptr<soot::EnvironmentObject> &env) -> soot::Object {
                return builtin_get_enum_vals(form, args, env);
            },
            &spec);
    }

    // Built-in get-enum-vals. Returns a list of (symbol . value) pairs
    // for the enum whose name is given as the first argument.
    soot::Object
    Compiler::builtin_get_enum_vals(const soot::Object &form, soot::Arguments &args,
                                    const std::shared_ptr<soot::EnvironmentObject> &env) {
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

        std::vector<soot::Object> enum_vals;
        for (auto &thing : sorted_values) {
            enum_vals.push_back(soot::Object::make_pair(
                soot::Object::make_symbol(&m_soot.symbol_table(), thing.first),
                soot::Object::make_integer(thing.second)));
        }

        return soot::build_list(enum_vals);
    }

    // Parse arguments into a soot::Arguments format. Throws a compiler error
    // if parsing fails.
    soot::Arguments Compiler::get_va(const soot::Object &form, const soot::Object &rest) {
        soot::Arguments args;
        std::string     err;
        if (!soot::get_va(rest, &err, &args)) { throw_compiler_error(form, "{}", err); }
        return args;
    }

    // Same as get_va, but does not handle named arguments.
    soot::Arguments Compiler::get_va_no_named(const soot::Object &form, const soot::Object &rest) {
        (void)form;
        soot::Arguments args;
        soot::get_va_no_named(rest, &args);
        return args;
    }

    // Validate the parsed arguments (unnamed/named) and throw a compiler
    // error if the check fails.
    void Compiler::va_check(
        const soot::Object &form, const soot::Arguments &args,
        const std::vector<std::optional<soot::ObjectType>> &unnamed,
        const std::unordered_map<std::string, std::pair<bool, std::optional<soot::ObjectType>>>
            &named) {
        std::string err;
        if (!soot::va_check(args, unnamed, named, &err)) { throw_compiler_error(form, "{}", err); }
    }

    // Walk a proper list and invoke f on each element. Throws a compiler
    // error if the list is not proper.
    void Compiler::for_each_in_list(const soot::Object                              &list,
                                    const std::function<void(const soot::Object &)> &f) {
        const soot::Object *iter = &list;
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
    Compiler::compile_file(soot::Object &forms, const std::string &filename) {
        m_current_file = filename;
        auto result = compile_internal(forms, filename);

        if (result && m_config.debug_print_ir) {
            lg::info("Compilation successful for: {}", filename);
            BinaryFileInspector inspector(result->get());
            inspector.inspect();
        }

        return result;
    }

    // The core compilation driver: build the node tree, generate a binary,
    // and wrap it in a BinaryFile. Returns nullptr (wrapped in a unique_ptr)
    // if the input produced no binary output (compile-time-only declarations).
    std::expected<std::unique_ptr<BinaryFile>, std::string>
    Compiler::compile_internal(soot::Object &forms, const std::string &filename) {
        try {
            NodeBuilder builder(m_ts, this);
            auto        file_node = std::make_unique<FileNode>(filename);

            // Top-level is a sequence of forms. Functions and declarations
            // become children of the FileNode; everything else goes into
            // a synthetic "top-level" function body.
            auto top_level_body = std::make_unique<SequenceNode>();
            auto top_level = std::make_unique<FunctionNode>("top-level");
            bool top_level_used = false;

            auto current = forms;
            while (current.is_pair()) {
                auto node = builder.build(current.as_pair()->car, file_node.get());
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

            // Generate the binary.
            GlobalState state;
            auto        element = file_node->generate(state);

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
            if (!binary_result) { return std::unexpected("Failed to create binary from buffer"); }

            auto binary = std::make_unique<BinaryFile>(std::move(binary_result.value()));

            // Copy data-struct layouts collected by FileNode into the BinaryFile
            // so that BinaryFileInspector can decode payloads without TypeSystem.
            for (const auto &ds : file_node->data_structs()) {
                binary->m_dataStructs.push_back(ds);
            }

            if (m_config.debug_print_asm) { color_binary_file(binary); }

            return binary;

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
                   "Please report this with the .soc file that triggered it.\n\n");
    }

    // ========== Interpretation ==========

    // Handle REPL commands that start with ':'. Unknown commands produce a
    // warning and are otherwise ignored.
    ReplStatus Compiler::handle_repl_command(const std::string &input) {
        // The input is already trimmed by handle_repl_string, but be defensive.
        std::string trimmed = input;
        const auto  first_ws = trimmed.find_first_not_of(" \t");
        if (first_ws != std::string::npos && first_ws > 0) { trimmed = trimmed.substr(first_ws); }

        if (trimmed == ":exit" || trimmed == ":quit") { return ReplStatus::WANT_EXIT; }

        if (trimmed == ":reload") { return ReplStatus::WANT_RELOAD; }

        // === :run <name> [args...] -- execute a loaded function in the VM ===
        if (trimmed.size() >= 5 && trimmed.substr(0, 5) == ":run ") {
            std::string rest = trimmed.substr(5);

            // Trim whitespace
            const auto first = rest.find_first_not_of(" \t");
            if (first == std::string::npos) {
                fmt::print(fg(fmt::color::yellow), "; ERROR: :run requires a function name\n");
                return ReplStatus::OK;
            }
            const auto last = rest.find_last_not_of(" \t");
            rest = rest.substr(first, last - first + 1);

            // Split into name and args by first space
            const auto  space = rest.find_first_of(" \t");
            std::string name = (space == std::string::npos) ? rest : rest.substr(0, space);
            std::string args_str = (space == std::string::npos) ? "" : rest.substr(space + 1);

            auto &globals = carbon::Globals::inst();
            void *fn_ptr = globals.find_symbol_ptr(carbon::StringId(name));

            if (fn_ptr == nullptr) {
                fmt::print(fg(fmt::color::crimson), "; ERROR: '{}' not found in Globals\n", name);
                return ReplStatus::OK;
            }

            auto *lambda = reinterpret_cast<carbon::ScriptLambda *>(fn_ptr);

            // Parse arguments — int (no dot) or float (with dot).
            std::vector<carbon::Variant> vm_args;
            if (!args_str.empty()) {
                std::istringstream iss(args_str);
                std::string        token;
                while (iss >> token) {
                    try {
                        if (token.find('.') != std::string::npos) {
                            vm_args.push_back(carbon::Variant(std::stod(token)));
                        } else {
                            vm_args.push_back(
                                carbon::Variant(static_cast<int64_t>(std::stoll(token))));
                        }
                    } catch (const std::exception &e) {
                        fmt::print(fg(fmt::color::yellow),
                                   "; WARN: skipping bad argument '{}': {}\n", token, e.what());
                    }
                }
            }

            carbon::VirtualMachine vm;
            carbon::Variant result = vm.execute_function(lambda, carbon::RunMode::Run, vm_args);

            fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "; {} => {}\n", rest,
                       result.to_string());
            return ReplStatus::OK;
        }

        if (trimmed == ":help") {
            m_repl->print_help_message();
            return ReplStatus::OK;
        }

        if (trimmed == ":clear") {
            m_repl->clear_screen();
            return ReplStatus::OK;
        }

        if (trimmed == ":sizes") {
            carbon::print_all_struct_sizes();
            return ReplStatus::OK;
        }

        // === :load <file> -- compile and register a module ===
        if (trimmed.substr(0, 5) == ":load") {
            std::string filename = trimmed.substr(6);
            filename.erase(0, filename.find_first_not_of(" \t"));
            filename.erase(filename.find_last_not_of(" \t") + 1);

            try {
                auto load_result = compile_file(filename);
                if (load_result) {
                    lg::info("Loaded and compiled: {}", filename);
                    carbon::Globals::inst().load_module(std::move(**load_result));
                } else {
                    lg::error("Failed to load: {}", load_result.error());
                }
            } catch (const std::exception &e) { print_error("Load error", e); }
            return ReplStatus::OK;
        }

        // === :soot <expr> -- evaluate a SOOT expression in the interpreter ===
        if (trimmed.size() >= 5 && trimmed.substr(0, 5) == ":soot") {
            std::string expr = trimmed.size() > 6 ? trimmed.substr(6) : "";
            if (expr.empty()) {
                fmt::print("; usage: :soot <expression>\n");
                return ReplStatus::OK;
            }
            try {
                auto result = m_soot.eval_string(expr, "<repl>");
                fmt::print(fg(fmt::color::green), "{}\n", result.print());
            } catch (const std::exception &e) {
                fmt::print(fg(fmt::color::crimson), "SOOT error: {}\n", e.what());
            }
            return ReplStatus::OK;
        }

        // === :list -- list symbols registered in Globals ===
        if (trimmed == ":list") {
            auto symbols = carbon::Globals::inst().all_symbols();
            for (auto &s : symbols) { fmt::print("  {}\n", s.to_cstring()); }
            return ReplStatus::OK;
        }

        lg::warn("Unknown command: {}", trimmed);
        return ReplStatus::OK;
    }

    // Dispatch a REPL line: commands go to handle_repl_command, everything
    // else is interpreted or compiled depending on the mode.
    ReplStatus Compiler::handle_repl_string(const std::string &input) {
        // Trim leading whitespace — the user may press space before typing.
        const auto first = input.find_first_not_of(" \t");
        if (first == std::string::npos) return ReplStatus::OK; // all whitespace

        std::string trimmed = input.substr(first);

        if (trimmed[0] == ':') return handle_repl_command(trimmed);

        switch (m_config.mode) {
        case CompilerMode::INTERPRET_ONLY: return interpret_and_print(trimmed);
        case CompilerMode::COMPILE_ONLY:
        case CompilerMode::HYBRID:
        default: return compile_and_report(trimmed);
        }
    }

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

            auto result = compile_file(forms, "<repl>");
            if (!result) {
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; ERROR: {}\n",
                           result.error());
                return ReplStatus::ERR;
            }

            // If the input contained only type/enum declarations, there is
            // no binary to register.
            if (!*result) {
                fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                           "; OK (types/enums registered)\n");
                return ReplStatus::OK;
            }

            print_listing(**result);

            bool loaded = carbon::Globals::inst().load_module(std::move(**result));
            if (!loaded) {
                fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold,
                           "; WARN: failed to register module in Globals\n");
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
    soot::Object Compiler::interpret(const std::string &input) {
        auto forms = m_soot.get_reader().read_from_string(input, false, "<repl>");
        return interpret(forms);
    }

    // Interpret already-parsed forms in the global SOOT environment.
    soot::Object Compiler::interpret(const soot::Object &forms) {
        if (m_config.debug_print_ast) { lg::info("AST: {}", forms.print()); }

        soot::Object result = soot::Object::make_none();
        try {
            auto env = m_soot.get_global_environment();

            if (forms.is_pair()) {
                for_each_in_list(forms, [&](const soot::Object &o) {
                    result = m_soot.eval_form(o, env.as_env_ptr());
                });
            } else {
                result = m_soot.eval_form(forms, env.as_env_ptr());
            }

            printf("%s\n", result.print().c_str());
            return result;

        } catch (soot::ExitException &e) {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nExit: {}\n", e.what());
            exit(e.exit_code);
        } catch (soot::EvalException &e) {
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

    void Compiler::set_global(const std::string &name, const soot::Object &value) {
        m_soot.define_global(name.c_str(), value);
    }

    soot::Object Compiler::get_global(const std::string &name) {
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

        fmt::print(fg(fmt::color::green), "; wrote {}\n", out.string());
        return true;
    }

    bool Compiler::is_soot_macro(const std::string &name) {
        auto sym = m_soot.intern(name.c_str());
        return try_getting_macro_from_soot(m_soot, sym, nullptr);
    }

    soot::Object Compiler::expand_soot_macro(const soot::Object &form) {
        return expand_soot_macro_for_compiler(m_soot, form);
    }

    // ===============================================================
    // Prelude loading
    // ===============================================================

    // Load lib.sot into the SOOT interpreter.
    // lib.sot is INTERPRETED: it contains utilities used while expanding
    // macros (car, cdr, map, filter, setf, defstruct, ...).
    void Compiler::load_soot_prelude() {
        namespace fs = std::filesystem;

        static std::set<fs::path> used_paths;

        std::vector<fs::path> candidates = {
            file_util::get_path(file_util::PathType::PROJECT) / "soot_src" / "lib.sot",
            file_util::get_path(file_util::PathType::CONFIG) / "soot_src" / "lib.sot",
            file_util::get_path(file_util::PathType::SHARE) / "soot_src" / "lib.sot",
        };

        for (const auto &p : candidates) {
            if (!fs::exists(p)) { continue; }

            // Normalize the path so that "a/b/c" and "./a/b/c" map to the same key.
            std::error_code ec;
            fs::path        normalized = fs::weakly_canonical(p, ec);
            if (ec) { normalized = p; }

            if (used_paths.contains(normalized)) {
                lg::info("Skipping already loaded SOOT library {}", p.string());
                continue;
            }
            used_paths.insert(normalized);

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

        lg::warn("lib.sot not found");
    }

    // Load lib.soc into the compiler.
    // lib.soc is COMPILED, not interpreted. Its macros register in m_soot,
    // its types register in m_ts.
    void Compiler::load_soc_prelude() {
        namespace fs = std::filesystem;

        static std::set<fs::path> used_paths;

        std::vector<fs::path> candidates = {
            file_util::get_path(file_util::PathType::PROJECT) / "soot_src" / "lib.soc",
            file_util::get_path(file_util::PathType::CONFIG) / "soot_src" / "lib.soc",
            file_util::get_path(file_util::PathType::SHARE) / "soot_src" / "lib.soc",
        };

        for (const auto &p : candidates) {
            if (!fs::exists(p)) { continue; }

            std::error_code ec;
            fs::path        normalized = fs::weakly_canonical(p, ec);
            if (ec) { normalized = p; }

            if (used_paths.contains(normalized)) {
                lg::info("Skipping already loaded SOOTC library {}", p.string());
                continue;
            }
            used_paths.insert(normalized);

            lg::info("Loading SOOTC library {}", p.string());

            try {
                auto result = compile_file(p);

                if (!result) {
                    lg::error("Failed to compile {}", p.string());
                    return;
                }

                if (*result) { carbon::Globals::inst().load_module(std::move(**result)); }

                lg::info("Loaded SOOTC library {}", p.string());
                return;

            } catch (const std::exception &e) {
                lg::error("Exception while loading {}: {}", p.string(), e.what());
                return;
            }
        }

        lg::warn("lib.soc not found");
    }
} // namespace sootc