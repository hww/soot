// sootc/compiler/Compiler.cpp
#include "sootc/compiler/Compiler.hpp"
#include "soot/PrettyPrinter.hpp"
#include "sootc/compiler/FileCompiler.hpp"
#include "sootc/compiler/NodeBuilder.hpp"
#include "sootc/node/FileNode.hpp"
#include "common/soot/Object.hpp"
#include "common/soot/Reader.hpp"
#include "common/soot/Interpreter.hpp"
#include "common/soot/ParseHelpers.hpp"
#include "common/util/Log.hpp"
#include "fmt/core.h"
#include "fmt/color.h"
#include "type_system/TypeSystem.hpp"

namespace sootc {

Compiler::Compiler(SootPlatform platform,
                   const CompilationOptions comp_options,
                   const std::optional<REPL::Config> repl_config,
                   const std::string& user_profile,
                   std::unique_ptr<REPL::Wrapper> repl) 
    : m_ts(TypeSystem::instance()),
      m_platform(platform),
      m_soot(user_profile, false),
      m_make(repl_config, user_profile),
      m_repl(std::move(repl))
{
    // Инициализация m_config
    m_config = comp_options;
    
    m_ts.add_builtin_types(m_platform);
    m_global_env = std::make_unique<GlobalNode>();
    m_none = std::make_unique<NoneNode>();

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
        m_repl->init_settings();
        using namespace std::placeholders;
        //m_repl->get_repl().set_completion_callback(std::bind(
        //    &Compiler::find_symbols_or_object_file_by_prefix, this, _1, _2, std::cref(examples)));
        //m_repl->get_repl().set_hint_callback(
        //    std::bind(&Compiler::find_hints_by_prefix, this, _1, _2, _3, std::cref(examples)));
        //m_repl->get_repl().set_highlighter_callback(
        //    std::bind(&Compiler::repl_coloring, this, _1, _2, std::cref(regex_colors)));
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
                soot::Object::make_symbol(thing.first),
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
    soot::Reader reader;
    auto forms = reader.read_from_string(content, false, path.string());
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
    }
    
    return result;
}

std::expected<std::unique_ptr<BinaryFile>, std::string> 
Compiler::compile_internal(soot::Object& forms, const std::string& filename) {
    try {
        NodeBuilder builder(m_ts, this);
        auto file_node = std::make_unique<FileNode>(filename);
        
        // Обход всех форм
        auto current = forms;
        while (current.is_pair()) {
            auto node = builder.build(current.as_pair()->car, file_node.get());
            if (node) {
                file_node->add_child(std::move(node));
            }
            current = current.as_pair()->cdr;
        }
        
        // Генерация бинарника
        GlobalState state;
        auto element = file_node->generate(state);
        
        auto bytes = make_aligned_buffer(element.m_rawData.size());
        std::memcpy(bytes.get(), element.m_rawData.data(), element.m_rawData.size());
        
        auto binary_result = BinaryFile::from_buffer(filename, std::move(bytes), element.m_rawData.size());
        if (!binary_result) {
            return std::unexpected("Failed to create binary from buffer");
        }
        
        auto binary = std::make_unique<BinaryFile>(std::move(binary_result.value()));
        
        // Color pass (регистровая аллокация если нужно)
        if (m_config.debug_print_asm) {
            color_binary_file(binary);
        }
        
        return binary;
        
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Compilation error: ") + e.what());
    }
}

// ========== Интерпретация ==========


ReplStatus Compiler::handle_repl_command(const std::string& input) {
    if (input == ":exit" || input == ":quit") {
        return ReplStatus::WANT_EXIT;
    }
    
    if (input == ":reload") {
        return ReplStatus::WANT_RELOAD;
    }
    
    if (input == ":help") {
        m_repl->print_help_message();
        return ReplStatus::OK;
    }
    
    if (input == ":clear") {
        m_repl->clear_screen();
        return ReplStatus::OK;
    }
    
    if (input.substr(0, 5) == ":load") {
        std::string filename = input.substr(6);
        filename.erase(0, filename.find_first_not_of(" \t"));
        filename.erase(filename.find_last_not_of(" \t") + 1);
        
        try {
            auto result = compile_file(filename);
            if (result) {
                lg::info("Loaded and compiled: {}", filename);
            } else {
                lg::error("Failed to load: {}", result.error());
            }
        } catch (const std::exception& e) {
            print_error("Load error", e);
        }
        return ReplStatus::OK;
    }
    
    lg::warn("Unknown command: {}", input);
    return ReplStatus::OK;
}

ReplStatus Compiler::handle_repl_string(const std::string& input) {
    if (input.empty()) return ReplStatus::OK;
    
    // Специальные команды
    if (input[0] == ':') {
        return handle_repl_command(input);
    }
    
    try {
        switch (m_config.mode) {
            case CompilerMode::INTERPRET_ONLY:
                return interpret_and_print(input);
                
            case CompilerMode::COMPILE_ONLY:
                return compile_and_report(input);
                
            case CompilerMode::HYBRID:
                return try_interpret_then_compile(input);
        }
    } catch (const std::exception& e) {
        print_error("Evaluation error", e);
        return ReplStatus::ERROR;
    }
    
    return ReplStatus::OK;
}

void Compiler::save_repl_history() {
  m_repl->save_history();
}

void Compiler::print_to_repl(const std::string& str) {
  m_repl->print_to_repl(str);
}

std::string Compiler::get_prompt() {
  std::string prompt = fmt::format(fmt::emphasis::bold | fg(fmt::color::cyan), "g > ");
  return "\033[0m" + prompt;
}

std::string Compiler::get_repl_input() {
  auto str = m_repl->readline(get_prompt());
  if (str) {
    m_repl->add_to_history(str);
    return str;
  } else {
    return "";
  }
}

// Вспомогательные методы
ReplStatus Compiler::interpret_and_print(const std::string& code) {
    auto result = interpret(code);
    if (m_config.debug_print_ast) {
        lg::info("=> {}", result.print());
    }
    return ReplStatus::OK;
}

ReplStatus Compiler::compile_and_report(const std::string& code) {
    auto forms = m_soot.get_reader().read_from_string(code, false, "<repl>");
    auto binary = compile_file(forms, "<repl>");
    if (!binary) {
        throw std::runtime_error("Compilation failed");
    }
    if (m_config.debug_print_asm) {
        lg::info("Compilation successful");
    }
    return ReplStatus::OK;
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

    soot::Object resutl = soot::Object::make_none();
    try {
        if (forms.is_pair()) {
            for_each_in_list(forms, [&](const soot::Object& o) {
                resutl = m_soot.eval_form(o);
            });
        } else {
             resutl = m_soot.eval_form(forms);
        }
        // print
        printf("%s\n", resutl.print().c_str());
        return resutl;
        
    } catch (soot::ExitException &e) {
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nExit: {}\n", e.what());
        exit(e.exit_code);
    } catch (soot::EvalException &e) {
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nError:");
        fmt::print("Error: {}", e.full_report(m_soot.get_reader()));
    } catch (const std::exception &e) {
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nError: {}\n", e.what());
    }
    return resutl;
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
    return m_soot.get_global(Object::intern(name.c_str()));
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

std::string Compiler::find_file(const std::string& filename) {
    namespace fs = std::filesystem;
    
    if (fs::exists(filename)) {
        return filename;
    }
    
    // Используем asm_file_search_dirs из REPL конфигурации
    for (const auto& dir : m_config.search_paths) {
        std::string candidate = fs::path(dir) / filename;
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    
    // Также проверяем наши пути поиска
    for (const auto& dir : m_config.search_paths) {
        std::string candidate = fs::path(dir) / filename;
        if (fs::exists(candidate)) {
            return candidate;
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

} // namespace sootc