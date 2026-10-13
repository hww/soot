
#include "carbon/vm/NativeFunc.hpp"
#include "CommonTypes.hpp"
#include "util/Log.hpp"
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

    // ============================================================================
    // Registry Initialization
    // ============================================================================

    void NativeFunctionRegistry::initialize_builtins() {
        // Basic I/O
        register_function("_print", native_print);
        register_function("_println", native_println);

     
        lg::info("Initialized {} built-in native functions", functions_.size());
    }

} // namespace vm