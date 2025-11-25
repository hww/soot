#include "native_func.hpp"
#include "util/log.h"
#include <iostream>

namespace vm {

    // ============================================================================
    // Native Function Registry Implementation
    // ============================================================================

    NativeFunctionRegistry& NativeFunctionRegistry::get_instance() {
        static NativeFunctionRegistry instance;
        return instance;
    }

    void NativeFunctionRegistry::register_function(StringId name, NativeFunction func) {
        functions_[name] = func;
        lg::debug("Registered native function: {}", string_id::to_string(name));
    }

    void NativeFunctionRegistry::register_function(const std::string& name, NativeFunction func) {
        register_function(string_id::register_string(name), func);
    }

    NativeFunction NativeFunctionRegistry::find_function(StringId name) const {
        auto it = functions_.find(name);
        return it != functions_.end() ? it->second : nullptr;
    }

    NativeFunction NativeFunctionRegistry::find_function(const std::string& name) const {
        return find_function(string_id::register_string(name));
    }

    // ============================================================================
    // Built-in Native Functions
    // ============================================================================

    Variant native_print(u32 argc, const Variant* argv) {
        for (u32 i = 0; i < argc; i++) {
            std::cout << argv[i].to_string() << " ";
        }
        return Variant(true);
    }

    Variant native_println(u32 argc, const Variant* argv) {
        native_print(argc, argv);
        std::cout << std::endl;
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
            s32 divisor = argv[1].to_int();
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
        register_function("print", native_print);
        register_function("println", native_println);

        // Arithmetic
        register_function("add", native_add);
        register_function("sub", native_subtract);
        register_function("mul", native_multiply);
        register_function("div", native_divide);

        // Math functions
        register_function("abs", [](u32 argc, const Variant* argv) -> Variant {
            if (argc < 1) return Variant(0);
            if (argv[0].is_float()) {
                return Variant(std::abs(argv[0].to_float()));
            }
            else {
                return Variant(std::abs(argv[0].to_int()));
            }
            });

        register_function("sqrt", [](u32 argc, const Variant* argv) -> Variant {
            if (argc < 1) return Variant(0.0f);
            f32 value = argv[0].to_float();
            if (value < 0) {
                lg::warn("sqrt called with negative value: {}", value);
                return Variant(0.0f);
            }
            return Variant(std::sqrt(value));
            });

        // Type conversion
        register_function("to_int", [](u32 argc, const Variant* argv) -> Variant {
            if (argc < 1) return Variant(0);
            return Variant(argv[0].to_int());
            });

        register_function("to_float", [](u32 argc, const Variant* argv) -> Variant {
            if (argc < 1) return Variant(0.0f);
            return Variant(argv[0].to_float());
            });

        lg::info("Initialized {} built-in native functions", functions_.size());
    }

} // namespace vm