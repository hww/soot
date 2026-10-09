
#include "common/carbon/kernel/NativeFunc.hpp"
#include "common/CommonTypes.hpp"
#include "common/util/Log.hpp"
#include "lib/StringId.hpp"
#include "fmt/args.h"        // ← ДОБАВЬ ЭТУ СТРОКУ
#include "fmt/format.h"      // ← если ещё нет
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
    Variant native_print(u32 argc, const Variant *argv) {
        if (argc == 0) { return Variant(true); }

        // ---- argv[0] — format string ----
        if (!argv[0].is_ptr()) {
            // Если первый аргумент не строка — печатаем всё как есть
            // (чтобы не падать на (print 42)).
            for (u32 i = 0; i < argc; ++i) {
                if (i > 0) fmt::print(" ");
                fmt::print("{}", argv[i]);
            }
            return Variant(true);
        }

        const char *fmt_str = static_cast<const char *>(argv[0].get_ptr());

        // ---- Остальные argv[1..N] — varargs для fmt ----
        fmt::dynamic_format_arg_store<fmt::format_context> store;
        for (u32 i = 1; i < argc; ++i) {
            const auto &v = argv[i];
            switch (v.get_type()) {
            case carbon::RuntimeType::Int: store.push_back(v.get_i64()); break;
            case carbon::RuntimeType::Float: store.push_back(v.get_f64()); break;
            case carbon::RuntimeType::Pointer: {
                const void *raw = v.get_ptr();
                if (raw == nullptr) {
                    store.push_back("null");
                } else {
                    // Соглашение: pointer в print — это C-string.
                    store.push_back(static_cast<const char *>(raw));
                }
                break;
            }
            case carbon::RuntimeType::Null: store.push_back("null"); break;
            }
        }

        // ---- Один вызов fmt::vprint — печатает всё скопом ----
        try {
            fmt::vprint(fmt_str, store);
        } catch (const fmt::format_error &e) {
            // Если формат сломан — печатаем как есть + сообщение.
            fmt::print(stderr, "[print format error: {}]\n", e.what());
            fmt::print("{}", fmt_str);
        }

        return Variant(true);
    }

    Variant native_println(u32 argc, const Variant *argv) {
        native_print(argc, argv);
        fmt::print("\n");
        return Variant(true);
    }

    Variant native_add(u32 argc, const Variant* argv) {
        if (argc < 2) return Variant(0);

        // Автоматическое приведение типов
        if (argv[0].is_float() || argv[1].is_float()) {
            return Variant(argv[0].to_float() + argv[1].to_float());
        }
        else {
            return Variant(argv[0].to_int() + argv[1].to_int());
        }
    }

    Variant native_subtract(u32 argc, const Variant* argv) {
        if (argc < 2) return Variant(0);

        if (argv[0].is_float() || argv[1].is_float()) {
            return Variant(argv[0].to_float() - argv[1].to_float());
        }
        else {
            return Variant(argv[0].to_int() - argv[1].to_int());
        }
    }

    Variant native_multiply(u32 argc, const Variant* argv) {
        if (argc < 2) return Variant(0);

        if (argv[0].is_float() || argv[1].is_float()) {
            return Variant(argv[0].to_float() * argv[1].to_float());
        }
        else {
            return Variant(argv[0].to_int() * argv[1].to_int());
        }
    }

    Variant native_divide(u32 argc, const Variant* argv) {
        if (argc < 2) return Variant(0);

        if (argv[0].is_float() || argv[1].is_float()) {
            f32 divisor = argv[1].to_float();
            if (divisor == 0.0f) {
                lg::warn("Division by zero in native function");
                return Variant(0.0f);
            }
            return Variant(argv[0].to_float() / divisor);
        }
        else {
            i32 divisor = argv[1].to_int();
            if (divisor == 0) {
                lg::warn("Division by zero in native function");
                return Variant(0);
            }
            return Variant(argv[0].to_int() / divisor);
        }
    }

    // ============================================================================
    // Registry Initialization
    // ============================================================================

    void NativeFunctionRegistry::initialize_builtins() {
        // Basic I/O
        register_function("_print", native_print);
        register_function("_println", native_println);

        // Arithmetic
        register_function("_add", native_add);
        register_function("_sub", native_subtract);
        register_function("_mul", native_multiply);
        register_function("_div", native_divide);

        // Math functions
        register_function("_abs", [](u32 argc, const Variant* argv) -> Variant {
            if (argc < 1) return Variant(0);
            if (argv[0].is_float()) {
                return Variant(std::abs(argv[0].to_float()));
            }
            else {
                return Variant(std::abs(argv[0].to_int()));
            }
            });

        register_function("_sqrt", [](u32 argc, const Variant* argv) -> Variant {
            if (argc < 1) return Variant(0.0f);
            f32 value = argv[0].to_float();
            if (value < 0) {
                lg::warn("sqrt called with negative value: {}", value);
                return Variant(0.0f);
            }
            return Variant(std::sqrt(value));
            });

        // Type conversion
        register_function("_to_int", [](u32 argc, const Variant* argv) -> Variant {
            if (argc < 1) return Variant(0);
            return Variant(argv[0].to_int());
            });

        register_function("_to_float", [](u32 argc, const Variant* argv) -> Variant {
            if (argc < 1) return Variant(0.0f);
            return Variant(argv[0].to_float());
            });

        lg::info("Initialized {} built-in native functions", functions_.size());
    }

} // namespace vm