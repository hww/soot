
#include "carbon/vm/NativeFunc.hpp"
#include "carbon/vm/NativeProcess.hpp"
#include "CommonTypes.hpp"
#include "util/Log.hpp"
#include "lib/StringId.hpp"
#include "fmt/format.h"       // fmt::format, fmt::vformat, fmt::dynamic_format_arg_store
#include "fmt/args.h"         // fmt::dynamic_format_arg_store (в некоторых версиях fmt)
#include "fmt/core.h"         // базовые функции fmt (fmt::print)     
#include <iostream>

namespace carbon {

    // ============================================================================
    // Native Function Registry Implementation
    // ============================================================================

    NativeFunctionRegistry& NativeFunctionRegistry::get_instance() {
        static NativeFunctionRegistry instance;
        return instance;
    }
    
    
    void NativeFunctionRegistry::register_function(StringId name, NativeFunction func) {
        functions_[name] = func;
        lg::debug("Registered native function: {}", name);
    }

    void NativeFunctionRegistry::register_function(const std::string& name, NativeFunction func) {
        register_function(StringId(name), func);
    }

    void NativeFunctionRegistry::register_function(const char* name, NativeFunction func) {
        register_function(StringId(name), func);
    }

    NativeFunction NativeFunctionRegistry::find_function(const StringId name) const {
        auto it = functions_.find(name);
        return it != functions_.end() ? it->second : nullptr;
    }

    NativeFunction NativeFunctionRegistry::find_function_by_name(const std::string& name) const {
        return find_function(StringId(name));
    }

    // ============================================================================
    // Built-in Native Functions
    // ============================================================================
    // ---- Общая функция для форматирования аргументов ----
    static std::string format_impl(u32 argc, const Variant *argv, const char *func_name) {
        if (argc == 0) { return std::string(); }

        // ---- argv[0] — format string ----
        if (!argv[0].is_ptr()) {
            // Если первый аргумент не строка — печатаем всё как есть
            // (чтобы не падать на (print 42)).
            std::string result;
            for (u32 i = 0; i < argc; ++i) {
                if (i > 0) result += " ";
                result += fmt::format("{}", argv[i]);
            }
            return result;
        }

        const char *fmt_str = static_cast<const char *>(argv[0].get_ptr());

        // ---- Остальные argv[1..N] — varargs для fmt ----
        fmt::dynamic_format_arg_store<fmt::format_context> store;
        for (u32 i = 1; i < argc; ++i) {
            // Push the Variant itself, not its unpacked value. This way
            // fmt dispatches to formatter<Variant>, which understands
            // {:p}, {:sid}, {:t=...}, {:x}, {:b}, {:f}, and {}.
            store.push_back(argv[i]);
        }

        // ---- Один вызов fmt::vformat — формирует строку целиком ----
        try {
            return fmt::vformat(fmt_str, store);
        } catch (const fmt::format_error &e) {
            // Если формат сломан — бросаем исключение с сообщением.
            throw std::runtime_error(fmt::format("[{}] format string `{}` contains error: {}\n",
                                                     func_name, fmt_str, e.what()));
        }
    }

    // ---- native_print: использует format_impl и печатает результат ----
    Variant native_print(u32 argc, const Variant *argv) {
        fmt::print("{}", format_impl(argc, argv, "print"));
        return Variant(true);
    }

    // ---- native_println: печатает результат + перевод строки ----
    Variant native_println(u32 argc, const Variant *argv) {
        fmt::print("{}\n", format_impl(argc, argv, "println"));
        return Variant(true);
    }

    // ---- native_format: возвращает отформатированную строку ----
    Variant native_format(u32 argc, const Variant *argv) {
        return Variant(format_impl(argc, argv, "format"));
    }

    // ============================================================================
    // String ID
    // ============================================================================
    
    Variant native_sid_to_string(u32 argc, const Variant* argv) {
        if (argc == 0) { 
              throw std::runtime_error(fmt::format("sid_to_string expects arg0 sid, got nothing\n"));
        }
        auto arg0 = argv[0];
        if (!arg0.is_int()) {
            throw std::runtime_error(
                fmt::format("sid_to_string expects arg0 sid, got {}\n", arg0.to_string()));
        }
        auto str = StringIdManager::instance().get_string(arg0.to_int());
        return Variant(str);
    }

    Variant native_string_to_sid(u32 argc, const Variant* argv) {
        if (argc == 0) {
            throw std::runtime_error(fmt::format("string_to_sid expects arg0 sid, got nothing\n"));
        }
        auto arg0 = argv[0];
        if (!arg0.is_ptr()) {
            throw std::runtime_error(
                fmt::format("string_to_sid expects arg0 string, got {}\n", arg0.to_string()));
        }
        return Variant(StringId(argv[1].to_string()));
    }

    // ============================================================================
    // Registry Initialization
    // ============================================================================

    void NativeFunctionRegistry::initialize_builtins() {
        // Basic I/O
        register_function("print", native_print);
        register_function("println", native_println);
        register_function("format", native_println);

        // String ID
        register_function("string-to-sid", native_string_to_sid);
        register_function("sid-to-string", native_sid_to_string);

        // Process control
        initialize_process_natives();

        lg::info("Initialized {} built-in native functions", functions_.size());
    }

} // namespace vm