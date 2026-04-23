#include "common/carbon/kernel/Kernel.hpp"
#include "common/carbon/kernel/ProcessRunner.hpp"
#include "common/carbon/vm/VirtualMachine.hpp"
#include "common/soot/Reader.hpp"
#include "common/soot/Object.hpp"
#include "common/util/Log.hpp"
#include "fmt/color.h"
#include "lib/StringId.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

using namespace carbon;
using namespace soot;

namespace fs = std::filesystem;

Variant parse_argument(const char* arg) {
    // Пробуем как целое число
    char* endptr = nullptr;
    long long_val = strtol(arg, &endptr, 10);
    if (*endptr == '\0' && endptr != arg) {
        return Variant(static_cast<i32>(long_val));
    }
    
    // Пробуем как число с плавающей точкой
    double double_val = strtod(arg, &endptr);
    if (*endptr == '\0' && endptr != arg) {
        return Variant(static_cast<float>(double_val));
    }
    
    // Пробуем как boolean
    if (strcmp(arg, "true") == 0 || strcmp(arg, "#t") == 0) {
        return Variant(true);
    }
    if (strcmp(arg, "false") == 0 || strcmp(arg, "#f") == 0) {
        return Variant(false);
    }
    
    // Иначе строка
    return Variant(std::string(arg));
}

void print_usage(const char* program_name) {
    fmt::print(fg(fmt::color::cyan), "Carbon VM - A lightweight Lisp-like virtual machine\n\n");
    fmt::print(fg(fmt::color::cyan), "Usage:\n");
    fmt::print("  {} [options] <module> [function] [args...]\n", program_name);
    fmt::print("  {} [options] -c <expression>\n", program_name);
    fmt::print("\n");
    fmt::print(fg(fmt::color::cyan), "Options:\n");
    fmt::print("  -I, --include <path>   Add module search path (can be used multiple times)\n");
    fmt::print("  -l, --load <module>    Load module (can be used multiple times)\n");
    fmt::print("  -c, --call <expr>      Call expression (e.g., '(add 5 3)')\n");
    fmt::print("  -h, --help             Show this help\n");
    fmt::print("\n");
    fmt::print(fg(fmt::color::cyan), "Examples:\n");
    fmt::print("  {} -I build/modules math/add 5 3\n", program_name);
    fmt::print("  {} -I build/modules lib/math add 5 3\n", program_name);
    fmt::print("  {} -I build/modules -c '(add 5 3)'\n", program_name);
    fmt::print("  {} -I build/modules --load math/add --call add 5 3\n", program_name);
}

int main(int argc, char* argv[]) {
    /*
    fmt::print("=== DEBUG: argc = {} ===\n", argc);
    for (int i = 0; i < argc; i++) {
        fmt::print("  argv[{}] = '{}'\n", i, argv[i]);
    }
    
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::vector<std::string> include_paths;
    std::vector<std::string> modules_to_load;
    std::string call_expr;
    std::string module_name;
    std::string function_name;
    std::vector<Variant> args;
    
    // Парсим аргументы
    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];
        
        if (arg == "-I" || arg == "--include") {
            if (i + 1 < argc) {
                include_paths.push_back(argv[++i]);
                i++;
            } else {
                fmt::print(fg(fmt::color::red), "Error: {} requires an argument\n", arg);
                return 1;
            }
        }
        else if (arg == "-l" || arg == "--load") {
            if (i + 1 < argc) {
                modules_to_load.push_back(argv[++i]);
                i++;
            } else {
                fmt::print(fg(fmt::color::red), "Error: {} requires an argument\n", arg);
                return 1;
            }
        }
        else if (arg == "-c" || arg == "--call") {
            if (i + 1 < argc) {
                call_expr = argv[++i];
                i++;
            } else {
                fmt::print(fg(fmt::color::red), "Error: {} requires an argument\n", arg);
                return 1;
            }
        }
        else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg[0] == '-') {
            fmt::print(fg(fmt::color::red), "Unknown option: {}\n", arg);
            print_usage(argv[0]);
            return 1;
        }
        else {
            // Не-опция: определяем как модуль/функцию/аргументы
            if (module_name.empty()) {
                module_name = arg;
                i++;
            }
            else if (function_name.empty()) {
                // Проверяем, является ли аргумент числом
                char* endptr = nullptr;
                strtol(arg.c_str(), &endptr, 10);
                if (*endptr == '\0' && endptr != arg.c_str()) {
                    // Это число - значит это первый аргумент
                    function_name = module_name;
                    // Извлекаем имя функции из модуля
                    size_t pos = function_name.find_last_of("/");
                    if (pos != std::string::npos) {
                        function_name = function_name.substr(pos + 1);
                    }
                    args.push_back(parse_argument(arg.c_str()));
                    i++;
                } else {
                    // Это имя функции
                    function_name = arg;
                    i++;
                }
            }
            else {
                args.push_back(parse_argument(arg.c_str()));
                i++;
            }
        }
    }
    
    // Если не указано имя функции, используем имя модуля
    if (function_name.empty() && !module_name.empty()) {
        size_t pos = module_name.find_last_of("/");
        if (pos != std::string::npos) {
            function_name = module_name.substr(pos + 1);
        } else {
            function_name = module_name;
        }
    }
    
    lg::info("=== Carbon VM ===");
    
    // 1. Инициализируем модульную систему
    ModuleRegistry& registry = ModuleRegistry::instance();
    
    // Добавляем пути поиска
    if (include_paths.empty()) {
        // По умолчанию: текущая директория
        registry.add_search_path(".");
        lg::info("Added default include path: .");
    } else {
        for (const auto& path : include_paths) {
            if (fs::exists(path)) {
                registry.add_search_path(path);
                lg::info("Added include path: {}", path);
            } else {
                lg::warn("Include path does not exist: {}", path);
            }
        }
    }
    
    // 2. Сканируем модули
    registry.scan_and_index(true);
    
    // 3. Загружаем указанные модули (если есть)
    ModuleManager& mm = ModuleManager::instance();
    std::vector<std::shared_ptr<Module>> loaded_modules;
    
    for (const auto& mod_name : modules_to_load) {
        auto module = mm.load_module(SID(mod_name.c_str()));
        if (module && module->binary_file) {
            loaded_modules.push_back(module);
            lg::info("Loaded module: {}", mod_name);
        } else {
            lg::error("Failed to load module: {}", mod_name);
        }
    }
    
    // 4. Если указан call_expr, парсим и выполняем
    if (!call_expr.empty()) {
        lg::info("Evaluating: {}", call_expr);
        
        // Инициализируем парсер
        Reader reader;
        Object form = reader.read_from_string(call_expr, false, "<command-line>");
        lg::info("Parsed: {}", form.print());
        
        // TODO: Выполнить S-выражение в контексте загруженных модулей
        lg::info("Expression evaluation not yet implemented");
        return 0;
    }
    
    // 5. Иначе выполняем вызов функции из одного модуля
    if (module_name.empty()) {
        lg::error("No module specified");
        print_usage(argv[0]);
        return 1;
    }
    
    // Загружаем модуль
    auto loaded_module = mm.load_module(StringId(module_name.c_str()));
    if (!loaded_module) {
        lg::error("Module '{}' not found", module_name);
        lg::info("Available modules:");
        int count = 0;
        for (auto name : registry.get_available_modules()) {
            lg::info("  - {}", name);
            count++;
        }
        if (count == 0) {
            lg::info("  (no modules found)");
            lg::info("Hint: Check that .dci files exist in include paths");
        }
        return 1;
    }
    
    if (!loaded_module->binary_file) {
        lg::error("Module '{}' has no binary data", module_name);
        return 1;
    }
    lg::info("{}", loaded_module->inspect());
    // Находим функцию через API модуля
    FunctionDesc* function_code = loaded_module->resolve_function(SID(function_name.c_str()));
    if (!function_code) {
        lg::error("Function '{}' not found in module '{}'", function_name, module_name);
        
        // Показываем доступные экспорты
        lg::info("Available exports in module '{}':", module_name);
        for (const auto& [name, def] : loaded_module->export_table) {
            lg::info("  - {} (type: {})", name.to_string(), 
                     def ? def->name : "unknown");
        }
        return 1;
    }
    
    lg::info("Module: {}", module_name);
    lg::info("Function: {}", function_name);
    lg::info("Arguments: {}", args.size());
    
    // 6. Инициализируем ядро
    Kernel& kernel = Kernel::instance();
    if (!kernel.initialize()) {
        lg::error("Failed to initialize kernel");
        return 1;
    }
    
    // 7. Создаём процесс
    Process* process = ProcessRunner::spawn(
        StringId("test_process"),
        kernel.root(),
        nullptr, // No entry point, we'll execute manually
        nullptr
    );
    
    if (!process) {
        lg::error("Failed to create process");
        kernel.shutdown();
        return 1;
    }
    
    lg::info("Process created: {}", process->to_string());
    
    // 8. Создаём фрейм и выполняем
    VirtualMachine& vm = kernel.virtual_machine();
    auto frame = std::make_shared<StackFrame>(function_code, nullptr);
    
    // Устанавливаем аргументы
    frame->argc = static_cast<u32>(args.size());
    for (size_t i = 0; i < args.size() && i < MAX_ARGS; i++) {
        frame->set_argument(static_cast<u32>(i), args[i]);
    }
    
    // Выводим аргументы для отладки
    std::string args_str;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) args_str += ", ";
        args_str += args[i].to_string();
    }
    lg::info("Calling {}({})...", function_name, args_str);
    
    // Выполняем функцию
    Variant result = vm.execute(frame);
    lg::info("Result: {}", result.to_string());
    
    // 9. Очистка
    kernel.shutdown();
    */
    return 0;
}