// sootc/compiler/Compiler.cpp
#include "fmt/core.h"
#include "fmt/color.h"
#include "third_party/replxx/include/replxx.hxx"

#include "sootc/compiler/Compiler.hpp"
#include "sootc/compiler/FileCompiler.hpp"
#include "sootc/compiler/NodeBuilder.hpp"
#include "sootc/node/FileNode.hpp"
#include "common/soot/Object.hpp"
#include "common/soot/Reader.hpp"
#include "common/soot/Interpreter.hpp"
#include "common/soot/ParseHelpers.hpp"
#include "common/util/Log.hpp"
#include "carbon/file/BinaryFileInspector.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/vm/VirtualMachine.hpp"
#include "util/FileUtil.hpp"
#include "util/Log.hpp"
#include "type_system/TypeSystem.hpp"
#include "sootc/node/SequenceNode.hpp"
#include "sootc/compiler/CompilerError.hpp"
#include <file/SizeAssertions.hpp>
#include <sootc/node/EnumDeclarationNode.hpp>
#include <sootc/node/TypeDeclarationNode.hpp>
#include <sootc/node/DataDeclarationNode.hpp>

namespace sootc {

Compiler::Compiler(SootPlatform platform,
                   const CompilationOptions comp_options,
                   const std::optional<REPL::Config> repl_config,
                   const std::string& user_profile,
                   std::unique_ptr<REPL::Wrapper> repl) 
    : m_ts(TypeSystem::instance()),
      m_platform(platform),
      m_soot(user_profile, false, true, platform),
      m_make(repl_config, user_profile),
      m_repl(std::move(repl))
{
    // Инициализация m_config
    m_config = comp_options;
    
    m_ts.add_builtin_types();
    m_global_env = std::make_unique<GlobalNode>();
    m_none = std::make_unique<NoneNode>();

    // --- Загрузка SOOT-прелюдии (builtins.sot) ---
    load_soot_prelude();

    if (m_config.mode != CompilerMode::COMPILE_ONLY) {
        setup_repl();
    }
    
    if (user_profile != "#f") {
        load_user_profile();
    }

     // load auto-complete history, only if we are running in the interactive mode.
    if (m_repl) {
        m_repl->load_history();
        m_repl->print_welcome_message(m_make.get_loaded_projects());
        
        // Инициализируем examples и regex_colors в m_repl
        m_repl->examples = {
            "(define ", "(defun ", "(defmacro ", "(if ", "(when ", 
            "(unless ", "(cond ", "(let ", "(lambda ", "(set! ", 
            "(quote ", "(load \"", "(compile \"", "(deftype ", "(defenum "
        };
        // Инициализируем examples и regex_colors
        m_repl->regex_colors = {
            {";[^\n]*", replxx::Replxx::Color::BRIGHTCYAN},
            {"\"[^\"]*\"", replxx::Replxx::Color::GREEN},
            {"\\b[0-9]+\\b", replxx::Replxx::Color::YELLOW},
            {"\\b(defun|defmacro|if|when|unless|cond|let|lambda|set!|define|quote|deftype|defenum)\\b",
            replxx::Replxx::Color::BRIGHTMAGENTA},
            {"\\b#t\\b|\\b#f\\b", replxx::Replxx::Color::BRIGHTMAGENTA},
            {"[\\(\\)]", replxx::Replxx::Color::BRIGHTBLUE},
        };
            
        m_repl->init_settings();
        using namespace std::placeholders;
        // Completion callback
        m_repl->get_repl().set_completion_callback(
            std::bind(&Compiler::find_symbols_or_object_file_by_prefix, 
                    this, _1, _2, std::cref(m_repl->examples)));
        
        m_repl->get_repl().set_hint_callback(
            std::bind(&Compiler::find_hints_by_prefix, 
                    this, _1, _2, _3, std::cref(m_repl->examples)));
        
        m_repl->get_repl().set_highlighter_callback(
            std::bind(&Compiler::repl_coloring, 
                    this, _1, _2));
    }

    // add soot forms that get info from the compiler
    setup_soot_forms();
}

Compiler::~Compiler() = default;

// В Compiler.cpp или в setup_goos_forms()
void Compiler::setup_goos_forms() {
    // Создаем спецификацию аргументов: ожидаем 1 позиционный аргумент (имя enum)
    soot::ArgumentSpec spec(true,false);
    
    // Регистрируем custom form
    m_soot.add_custom_form("get-enum-vals", 
        [this](const soot::Object& form, soot::Arguments& args,
               const std::shared_ptr<soot::EnvironmentObject>& env) -> soot::Object {
            return builtin_get_enum_vals(form, args, env);
        },
        &spec
    );
}

/*! 
 * The method will be invoked for each enum form
 */
soot::Object Compiler::builtin_get_enum_vals(const soot::Object& form, 
                                               soot::Arguments& args,
                                               const std::shared_ptr<soot::EnvironmentObject>& env) {
    (void)form; (void)env;
    // Вычисляем аргументы (если нужно)
    // m_interpreter->eval_args(&args, env); // Раскомментируйте если аргументы нужно вычислить
    
    // Проверяем количество аргументов
    if (args.unnamed.empty()) {
        throw std::runtime_error("get-enum-vals: expected enum name as argument");
    }
    
    // Получаем имя enum из первого аргумента
    const auto& enum_obj = args.unnamed[0];
    if (!enum_obj.is_symbol()) {
        throw std::runtime_error("get-enum-vals: expected symbol as enum name");
    }
    
    const auto& enum_name = enum_obj.as_symbol().name_ptr;
    auto enum_type = m_ts.try_enum_lookup(enum_name);
    
    if (!enum_type) {
        throw std::runtime_error(fmt::format("get-enum-vals: unknown enum '{}'", enum_name));
    }
    
    // Собираем значения enum
    std::vector<std::pair<std::string, int64_t>> sorted_values;
    for (auto& val : enum_type->entries()) {
        sorted_values.emplace_back(
            val.first,
            enum_type->is_bitfield() ? static_cast<int64_t>(1) << val.second : val.second
        );
    }
    
    // Сортируем по значению
    std::sort(sorted_values.begin(), sorted_values.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });
    
    // Формируем список пар (symbol . value)
    std::vector<soot::Object> enum_vals;
    for (auto& thing : sorted_values) {
        enum_vals.push_back(
            soot::Object::make_pair(
                soot::Object::make_symbol(&m_soot.symbol_table(), thing.first),
                soot::Object::make_integer(thing.second)
            )
        );
    }
    
    return soot::build_list(enum_vals);
}

/*!
 * Parse arguments into a soot::Arguments format.
 */
soot::Arguments Compiler::get_va(const soot::Object& form, const soot::Object& rest) {
  soot::Arguments args;

  std::string err;
  if (!soot::get_va(rest, &err, &args)) {
    throw_compiler_error(form, "{}", err);
  }
  return args;
}

/*!
 * Parse arguments into a soot::Arguments format.
 */
soot::Arguments Compiler::get_va_no_named(const soot::Object& form, const soot::Object& rest) {
  (void)form;
  soot::Arguments args;
  soot::get_va_no_named(rest, &args);
  return args;
}

/*!
 * Check arguments in a soot::Arguments format (named and unnamed) and throw a compiler error if it
 * fails.
 */
void Compiler::va_check(
    const soot::Object& form,
    const soot::Arguments& args,
    const std::vector<std::optional<soot::ObjectType>>& unnamed,
    const std::unordered_map<std::string, std::pair<bool, std::optional<soot::ObjectType>>>&
        named) {
  std::string err;
  if (!soot::va_check(args, unnamed, named, &err)) {
    throw_compiler_error(form, "{}", err);
  }
}

/*!
 * Iterate through elements of a soot list and apply the given function. Throw compiler error if the
 * list is invalid.
 */
void Compiler::for_each_in_list(const soot::Object& list,
                                const std::function<void(const soot::Object&)>& f) {
  const soot::Object* iter = &list;
  while (iter->is_pair()) {
    auto lap = iter->as_pair();
    f(lap->car);
    iter = &lap->cdr;
  }

  if (!iter->is_null()) {
    throw_compiler_error(list, "Invalid list: {}", list.print());
  }
}

     
// ========== Компиляция ==========

std::expected<std::unique_ptr<BinaryFile>, std::string> 
Compiler::compile_file(const std::filesystem::path& path) {
    std::string content = read_file_content(path.string());
    auto forms = m_soot.get_reader().read_from_string(content, false, path.string());
    if (forms.is_null()) {
        return std::unexpected("Failed to read or parse file: " + path.string());
    }
    return compile_file(forms, path.string());
}

std::expected<std::unique_ptr<BinaryFile>, std::string> 
Compiler::compile_file(soot::Object& forms, const std::string& filename) {
    m_current_file = filename;
    auto result = compile_internal(forms, filename);
    
    if (result && m_config.debug_print_ir) {
        // Печать IR если нужно
        lg::info("Compilation successful for: {}", filename);
        BinaryFileInspector inspector(result->get());
        inspector.inspect();
    }
    
    return result;
}

std::expected<std::unique_ptr<BinaryFile>, std::string>
Compiler::compile_internal(soot::Object &forms, const std::string &filename) {
    try {
        NodeBuilder builder(m_ts, this);
        auto        file_node = std::make_unique<FileNode>(filename);

        // --- top-level: последовательность выражений верхнего уровня ---
        auto top_level_body = std::make_unique<SequenceNode>();
        auto top_level = std::make_unique<FunctionNode>("top-level");
        bool top_level_used = false;

        auto current = forms;
        while (current.is_pair()) {
            auto node = builder.build(current.as_pair()->car, file_node.get());
            if (node) {
                if (dynamic_cast<FunctionNode *>(node.get()) != nullptr) {
                    // Top-level function — separate child of FileNode.
                    file_node->add_child(std::move(node));
                } else if (dynamic_cast<TypeDeclarationNode *>(node.get()) != nullptr ||
                           dynamic_cast<EnumDeclarationNode *>(node.get()) != nullptr) {
                    // Type/enum declaration — compile-time only, no code.
                    file_node->add_child(std::move(node));
                } else if (dynamic_cast<DataDeclarationNode *>(node.get()) != nullptr) {
                    // Top-level data declaration: (define name (new Type ...))
                    file_node->add_child(std::move(node));
                } else if (dynamic_cast<NewNode *>(node.get()) != nullptr) {
                    // Bare (new ...) at top level is not allowed.
                    // It should be wrapped in (define ...).
                    throw CompilerError("Compiler::compile_internal")
                        .where(fmt::format("file '{}'", filename))
                        .expected("top-level 'new' to be wrapped in 'define'")
                        .got("bare (new ...)");
                } else if (auto *expr = dynamic_cast<ExpressionNode *>(node.get())) {
                    // Regular expression — goes into the top-level body.
                    node.release(); // ownership transfers to top_level_body
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

        // --- Генерация бинарника ---
        GlobalState state;
        auto        element = file_node->generate(state);

        // Nothing to emit — the file contained only compile-time declarations
        // (deftype / defenum). The types are already registered in TypeSystem,
        // so there is nothing to write to a .bin file.
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

        if (m_config.debug_print_asm) { color_binary_file(binary); }

        return binary;

    } catch (const std::exception &e) {
        return std::unexpected(std::string("Compilation error: ") + e.what());
    }
}

// ========== Интерпретация ==========


ReplStatus Compiler::handle_repl_command(const std::string &input) {
    if (input == ":exit" || input == ":quit") { return ReplStatus::WANT_EXIT; }

    if (input == ":reload") { return ReplStatus::WANT_RELOAD; }

    // === :run <name> — выполнить загруженную функцию в VM ===
    if (input.size() >= 5 && input.substr(0, 5) == ":run ") {
        std::string name = input.substr(5);

        // trim пробелов слева и справа
        const auto first = name.find_first_not_of(" \t");
        if (first == std::string::npos) {
            fmt::print(fg(fmt::color::yellow), "; ERROR: :run requires a function name\n");
            return ReplStatus::OK;
        }
        const auto last = name.find_last_not_of(" \t");
        name = name.substr(first, last - first + 1);

        auto &globals = carbon::Globals::inst();
        void *fn_ptr = globals.find_symbol_ptr(carbon::StringId(name));

        if (fn_ptr == nullptr) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: '{}' not found in Globals\n", name);
            return ReplStatus::OK;
        }

        auto *lambda = reinterpret_cast<carbon::ScriptLambda *>(fn_ptr);

        carbon::VirtualMachine vm;
        carbon::Variant        result = vm.execute_function(lambda, carbon::RunMode::Run);

        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "; {} => {}\n", name,
                   result.to_string());
        return ReplStatus::OK;
    }

    // === :help, :clear, :load — как было ===
    if (input == ":help") {
        m_repl->print_help_message();
        return ReplStatus::OK;
    }

    if (input == ":clear") {
        m_repl->clear_screen();
        return ReplStatus::OK;
    }

    if (input == ":sizes") { 
        carbon::print_all_struct_sizes();
        return ReplStatus::OK;
    }

    if (input.substr(0, 5) == ":load") {
        std::string filename = input.substr(6);
        filename.erase(0, filename.find_first_not_of(" \t"));
        filename.erase(filename.find_last_not_of(" \t") + 1);

        try {
            auto load_result = compile_file(filename);
            if (load_result) {
                lg::info("Loaded and compiled: {}", filename);
                // Компиляция также регистрирует функции в Globals
                // через compile_and_report; но :load использует compile_file
                // напрямую, поэтому регистрируем вручную:
                carbon::Globals::inst().load_module(std::move(**load_result));
            } else {
                lg::error("Failed to load: {}", load_result.error());
            }
        } catch (const std::exception &e) { print_error("Load error", e); }
        return ReplStatus::OK;
    }
    if (input.size() >= 5 && input.substr(0, 5) == ":soot") {
        std::string expr = input.size() > 6 ? input.substr(6) : "";
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
    if (input == ":list") {
        auto symbols = carbon::Globals::inst().all_symbols();
        for (auto &s : symbols) { fmt::print("  {}\n", s.to_cstring()); }
        return ReplStatus::OK;
    }

    lg::warn("Unknown command: {}", input);
    return ReplStatus::OK;
}

ReplStatus Compiler::handle_repl_string(const std::string& input) {
    if (input.empty()) return ReplStatus::OK;

    if (input[0] == ':') return handle_repl_command(input);

    switch (m_config.mode) {
        case CompilerMode::INTERPRET_ONLY:
            return interpret_and_print(input);
        case CompilerMode::COMPILE_ONLY:
        case CompilerMode::HYBRID:
        default:
            return compile_and_report(input);
    }
}

void Compiler::save_repl_history() {
  m_repl->save_history();
}

void Compiler::print_to_repl(const std::string& str) {
  m_repl->print_to_repl(str);
}

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

std::string Compiler::get_repl_input() {
    std::string result;
    bool        first_line = true;

    while (true) {
        const char *input;
        if (first_line) {
            input = m_repl->readline(get_prompt()); // "sci> "
            first_line = false;
        } else {
            input = m_repl->readline("      "); // 6 пробелов
        }

        if (!input) return "";

        std::string line(input);

        if (!result.empty()) { result += "\n"; }
        result += line;

        // Проверяем завершённость
        if (m_soot.get_reader().is_expression_complete(result)) {
            m_repl->add_to_history(result);
            return result;
        }

        // Пустая строка + незавершено = продолжаем
    }
}

// Вспомогательные методы
ReplStatus Compiler::interpret_and_print(const std::string& script) {
    auto result = interpret(script);
    if (m_config.debug_print_ast) {
        lg::info("=> {}", result.print());
    }
    return ReplStatus::OK;
}

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

        // ---- ПРОВЕРКА: только type/enum declarations? ----
        if (!*result) {
            fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                       "; OK (types/enums registered)\n");
            return ReplStatus::OK;
        }
        // --------------------------------------------------

        // Печатаем листинг (BinaryFileInspector::inspect внутри).
        print_listing(**result);

        // Передаём владение BinaryFile в Globals.
        bool loaded = carbon::Globals::inst().load_module(std::move(**result));
        if (!loaded) {
            fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold,
                       "; WARN: failed to register module in Globals\n");
        }

        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "; OK\n");
        return ReplStatus::OK;
    } catch (const std::exception &e) {
        fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "; EXCEPTION: {}\n", e.what());
        return ReplStatus::ERR;
    }
}

ReplStatus Compiler::try_interpret_then_compile(const std::string& code) {
    try {
        auto result = interpret(code);
        if (m_config.debug_print_ast) {
            lg::info("=> {}", result.print());
        }
        return ReplStatus::OK;
    } catch (const std::exception& e) {
        lg::debug("Interpret failed: {}, trying compilation", e.what());
        return compile_and_report(code);
    }
}

soot::Object Compiler::interpret(const std::string& input) {
    auto forms = m_soot.get_reader().read_from_string(input, false, "<repl>");
    return interpret(forms);
}

soot::Object Compiler::interpret(const soot::Object& forms) {
    if (m_config.debug_print_ast) {
        lg::info("AST: {}", forms.print());
    }

    soot::Object result = soot::Object::make_none();
    try {
        // Получаем глобальное окружение из m_soot
        auto env = m_soot.get_global_environment();
        
        if (forms.is_pair()) {
            for_each_in_list(forms, [&](const soot::Object& o) {
                result = m_soot.eval_form(o, env.as_env_ptr());  // Передаем окружение
            });
        } else {
            result = m_soot.eval_form(forms, env.as_env_ptr());  // Передаем окружение
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

// ========== Управление окружением ==========

void Compiler::load_user_profile() {
    if (m_config.user_profile == "#f") return;
    
    // Используем функцию из REPL для загрузки startup файла
    REPL::StartupFile startup = REPL::load_user_startup_file(
        m_config.user_profile, 
        SootPlatform::Default
    );
    
    // Выполняем команды из startup файла
    for (const auto& cmd : startup.run_before_listen) {
        try {
            auto result = interpret(cmd);
        } catch (const std::exception& e) {
            print_warning(fmt::format("Failed to execute startup command: {}", e.what()));
        }
    }
    
    lg::info("Loaded user profile: {}", m_config.user_profile);
}

// ========== Управление окружением ==========

void Compiler::set_global(const std::string& name, const soot::Object& value) {
    m_soot.define_global(name.c_str(), value);
}

soot::Object Compiler::get_global(const std::string& name) {
    return m_soot.get_global(Object::intern(&m_soot.symbol_table(), name.c_str()));
}

void Compiler::reload_environment() {
    lg::info("Reloading environment...");
    load_user_profile();
    lg::info("Environment reloaded");
}

// ========== Private методы ==========

void Compiler::setup_repl() {
    // Создаем REPL Wrapper - используем конструктор с GameVersion
    // Так как у нас нет username, config, startup, nrepl_alive,
    // используем простой конструктор
    
    m_repl = std::make_unique<REPL::Wrapper>(SootPlatform::Default);
    
    // Настраиваем конфигурацию
    m_repl->username = m_config.user_profile;
    
    // Инициализируем настройки
    m_repl->init_settings();
    
    // Загружаем историю
    m_repl->load_history();
}

void Compiler::setup_soot_forms() {
    // Регистрация дополнительных форм для soot (если нужно)
    // Аналог их get-enum-vals и т.д.
}

void Compiler::color_binary_file(std::unique_ptr<BinaryFile>& /*binary*/) {
    // Регистровая аллокация и оптимизации
    // Аналог их color_object_file
    if (m_config.debug_print_asm) {
        // binary->print_asm();
    }
}

std::vector<uint8_t> Compiler::codegen_binary(BinaryFile* binary) {
    // Финальная кодогенерация
    return {}; 
}

std::string Compiler::find_file(const std::string &filename) {
    namespace fs = std::filesystem;

    if (fs::exists(filename)) { return filename; }

    // Ищем по путям из конфигурации
    for (const auto &dir : m_config.search_paths) {
        fs::path candidate = fs::path(dir) / filename; // <-- fs::path, не std::string
        if (fs::exists(candidate)) {
            return candidate.string(); // <-- .string() при возврате
        }
    }

    return filename;
}

std::string Compiler::read_file_content(const std::string& filename) {
    std::string path = find_file(filename);
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    
    std::string content;
    file.seekg(0, std::ios::end);
    content.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(&content[0], content.size());
    return content;
}

void Compiler::print_error(const std::string& context, const std::exception& e) {
    // Вариант 1: Без цветов (если Log не поддерживает fmt::text_style)
    //lg::error("{}: {}", context, e.what());
    
    // Вариант 2: С цветами через fmt::print напрямую
    fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "{}: {}\n", context, e.what());
}

void Compiler::print_warning(const std::string& warning) {
    // Вариант 1: Без цветов
    //lg::warn("Warning: {}", warning);
    
    // Вариант 2: С цветами через fmt::print
    fmt::print(fg(fmt::color::yellow), "Warning: {}\n", warning);
}


// ===============================================================
//  REPL Callbacks 
// ===============================================================
replxx::Replxx::completions_t Compiler::find_symbols_or_object_file_by_prefix(
    const std::string& context,
    int& context_len,
    const std::vector<std::string>& examples) {
    
    replxx::Replxx::completions_t matches;
    
    if (context.empty()) {
        context_len = 0;
        return matches;
    }
    
    context_len = static_cast<int>(context.length());
    
    // Add matching examples
    for (const auto& ex : examples) {
        if (ex.find(context) == 0) {
            matches.emplace_back(ex);
        }
    }
    
    // Add matching symbols from interpreter
    auto symbols = m_soot.get_all_symbols_matching(context);
    for (const auto& sym : symbols) {
        matches.emplace_back(sym);
    }
    
    return matches;
}

replxx::Replxx::hints_t Compiler::find_hints_by_prefix(
    const std::string& context,
    int& context_len,
    replxx::Replxx::Color& color,
    const std::vector<std::string>& examples) {
    
    replxx::Replxx::hints_t hints;
    
    if (context.empty()) {
        context_len = 0;
        return hints;
    }
    
    context_len = static_cast<int>(context.length());
    color = replxx::Replxx::Color::BRIGHTCYAN;
    
    // Function signatures as hints
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
    
    // If no specific hints, try to complete from examples
    if (hints.empty()) {
        for (const auto& ex : examples) {
            if (ex.find(context) == 0 && ex != context) {
                hints.push_back(ex.substr(context.length()));
            }
        }
    }
    
    return hints;
}

void Compiler::repl_coloring(
    const std::string& input,
    replxx::Replxx::colors_t& colors) {
    
    // Ручной парсинг строки и установка цветов для каждого символа
    bool in_string = false;
    bool in_comment = false;
    
    for (size_t i = 0; i < input.size(); i++) {
        char c = input[i];
        
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
        } else if (std::isdigit(c) || (c == '-' && std::isdigit(input[i+1]))) {
            // цифры
            colors[i] = replxx::Replxx::Color::YELLOW;
        } else {
            // Проверка ключевых слов
            // (сложная логика для определения границ слов)
        }
    }
}
// ===============================================================
// Печать / сохранение
// ===============================================================


void Compiler::print_listing(const BinaryFile &file) {
    // BinaryFileInspector принимает BinaryFile* (не const), поэтому const_cast.
    // Если inspector реально не меняет file — можно сделать конструктор const.
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
    // const_cast — потому что save не const, но file не меняется логически
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

    // TODO(check API): при наличии BinaryFileInspector с выводом в поток —
    // перенаправьте его сюда. Пока — минимальный листинг.
    ofs << "; SOOT listing\n";
    ofs << "; (not yet wired to BinaryFile API)\n";
    (void)file;

    fmt::print(fg(fmt::color::green), "; wrote {}\n", out.string());
    return true;
}
// ===============================================================
// Печать / сохранение
// ===============================================================
bool Compiler::is_soot_macro(const std::string &name)  {
    auto sym = m_soot.get_global(name.c_str());
    return sym.is_macro();
}

soot::Object Compiler::expand_soot_macro(const soot::Object &form) {
    return m_soot.macroexpand(form);
}

// ===============================================================
// Загрузка файлов
// ===============================================================
void Compiler::load_soot_prelude() {
    namespace fs = std::filesystem;
    // Множество уже загруженных путей (нормализованных)
    static std::set<fs::path> used_paths;

    std::vector<fs::path> candidates = {
        file_util::get_path(file_util::PathType::PROJECT) / "soot_src" / "lib.sot",
        file_util::get_path(file_util::PathType::CONFIG) / "soot_src" / "lib.sot",
        file_util::get_path(file_util::PathType::SHARE) / "soot_src" / "lib.sot",
    };

    for (const auto &p : candidates) {
        if (fs::exists(p)) {
            try {

                // Нормализуем путь — чтобы "a/b/c" и "./a/b/c" считались одним и тем же
                std::error_code ec;
                fs::path        normalized = fs::weakly_canonical(p, ec);
                if (ec) {
                    normalized = p; // если не получилось — используем как есть
                }

                // Пропускаем, если уже загружали
                if (used_paths.contains(normalized)) {
                    lg::info("Skipping already loaded SOOTC library {}", p.string());
                    continue;
                }
                used_paths.insert(normalized);

                lg::info("Loading SOOT library {}", p.string());

                std::string content = file_util::read_text(p);

                // ─── Убрать UTF-8 BOM ───
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
    }

    lg::warn("lib.sot not found");
}

} // namespace sootc