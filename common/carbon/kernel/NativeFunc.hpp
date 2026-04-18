#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/lib/Variant.hpp"
#include <map>

using namespace carbon;

namespace carbon {

    /**
     * @brief Safe argument access for native functions
     * @example
     *   StringId obj_name = SC_ARG(0, get_as_sid, SID("default"));
     *   i32 value = SC_ARG(1, get_as_s32, 0);
     */
    #define SC_ARG(arg_num, T, default_val) \
        sc_arg_convert<T>(arg_num, argc, argv, SID(#T), (default_val))

    template<typename T>
    T sc_arg_convert(u32 arg_num, u32 argc, const Variant* argv,
        RuntimeType  expected_type, T default_val) {
        if (arg_num >= argc) return default_val;
        auto& arg = argv[arg_num];
        if (arg.is_null()) return default_val;

        auto actual_type = arg.get_type();

        if constexpr (std::is_same_v<T, i32>) {
            if (actual_type == RuntimeType ::Int) return arg.get_i32();
            if (actual_type == RuntimeType ::Float) return static_cast<i32>(arg.get_f32());
        }
        else if constexpr (std::is_same_v<T, i64>) {
            if (actual_type == RuntimeType ::Int) return arg.get_i64();
            if (actual_type == RuntimeType ::Float) return static_cast<i64>(arg.get_f64());
        }
        else if constexpr (std::is_same_v<T, u32>) {
            if (actual_type == RuntimeType ::Int) return arg.get_u32();
            if (actual_type == RuntimeType ::Float) return static_cast<u32>(arg.get_f32());
        }
        else if constexpr (std::is_same_v<T, u64>) {
            if (actual_type == RuntimeType ::Int) return arg.get_u64();
            if (actual_type == RuntimeType ::Float) return static_cast<u64>(arg.get_f64());
        }
        else if constexpr (std::is_same_v<T, f32>) {
            if (actual_type == RuntimeType ::Float) return arg.get_f32();
            if (actual_type == RuntimeType ::Int) return static_cast<f32>(arg.get_i64());
        }
        else if constexpr (std::is_same_v<T, f64>) {
            if (actual_type == RuntimeType ::Float) return arg.get_f64();
            if (actual_type == RuntimeType ::Int) return static_cast<f64>(arg.get_i64());
        }
        else if constexpr (std::is_same_v<T, bool>) {
            if (actual_type == RuntimeType ::Int) return arg.get_i64() != 0;
            if (actual_type == RuntimeType ::Float) return arg.get_f32() != 0.0f;
        }
        else if constexpr (std::is_same_v<T, StringId>) {
            if (actual_type == RuntimeType ::Int) return static_cast<StringId>(arg.get_i64());
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            if (actual_type == RuntimeType ::Pointer) return arg.to_string();
        }
        else if constexpr (std::is_pointer_v<T>) {
            if (actual_type == expected_type) {
                return static_cast<T>(arg.get_ptr());
            }
        }
        else {
            if (actual_type == expected_type) {
                return arg.get_ptr();
            }
        }

        throw TypeError("sc_arg_convert()", expected_type, actual_type);
        return default_val;
    }

     /**
      * @brief The native function type
      */
    using NativeFunction = Variant(*)(u32 argc, const Variant* argv);

    /**
     * @brief Native function registry
     */
    class NativeFunctionRegistry {
    public:
        static NativeFunctionRegistry& get_instance();

        void register_function(StringId name, NativeFunction func);
        void register_function(const std::string& name, NativeFunction func);
        void register_function(const char* name, NativeFunction func);

        NativeFunction find_function(StringId name) const;
        NativeFunction find_function_by_name(const std::string& name) const;
        bool function_exists(StringId name) const {
            return functions_.find(name) != functions_.end();
        }
        void initialize_builtins();
        int function_count() { return functions_.size(); }
    private:
        NativeFunctionRegistry() = default;
        std::map<StringId, NativeFunction> functions_;
    };

    // Convenience macros for native function registration
#define REGISTER_NATIVE_FUNCTION(name, func) \
        NativeFunctionRegistry::get_instance().register_function(name, func)

} // namespace vm