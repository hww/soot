#include "sootc/Compiler/FunctionCompiler.hpp"
#include "fmt/format.h"
#include "sootc/Compiler/Compiler.hpp"
#include "common/carbon/files/FunctionDesc.hpp" 
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "sootc/IR/IR_Value.hpp"
#include "sootc/IR/IR_Node.hpp"
#include "sootc/Env/MethodEnv.hpp"
#include "sootc/Env/StateEnv.hpp"
#include "sootc/Env/TypeEnv.hpp"
#include "sootc/IR/StaticObject.hpp"
#include "type_system/Type.hpp"

using namespace ::carbon::files;
using namespace ::carbon::vm;

namespace sootc {

int FunctionCompiler::s_lambda_index = 1;


FunctionCompiler::FunctionCompiler(TypeSystem& ts, Compiler* compiler)
    : ts_(ts), compiler_(compiler) {}

// ============================================================================
// compile_function — один проход
// ============================================================================

IR_Value* FunctionCompiler::compile_function(const script::Object& form, 
                                               const script::Object& rest, 
                                               Env* env) {
    (void)form;
    
    auto args_list = rest.as_pair()->car;
    auto body_forms = rest.as_pair()->cdr;



    auto* f_env = new FunctionEnv(env, "lambda");
    
    // Парсим аргументы
    parse_arguments(args_list, f_env);
    
    // Компилируем тело (создаем IR_Node)
    compile_body(body_forms, f_env);
    
    return new IR_FunctionValue(f_env);
}

// ============================================================================
// compile_method — один проход
// ============================================================================

IR_Value* FunctionCompiler::compile_method(const script::Object& form,
                                            const script::Object& rest,
                                            TypeEnv* type_env,
                                            int method_id) {
    (void)form;
    
    auto args_list = rest.as_pair()->car;
    auto body_forms = rest.as_pair()->cdr;
    
    std::string method_name = form.as_pair()->car.as_symbol().c_str();
    Type* type = type_env->get_type();

    MethodInfo method_info;                                              

    if (!ts_.try_lookup_method(type->get_name(), method_name, &method_info)) {
        throw std::runtime_error(fmt::format("Method '{}' not found in type '{}'", method_name, type->get_name()));
    }
    auto* m_env = new MethodEnv(method_info.id, method_name, type_env, type);
    
    // Парсим аргументы (this уже добавлен в конструкторе)
    parse_arguments(args_list, m_env);
    
    // Компилируем тело
    compile_body(body_forms, m_env);
    
    return new IR_MethodValue(m_env);
}

// ============================================================================
// compile_state — один проход
// ============================================================================

IR_Value* FunctionCompiler::compile_state(const script::Object& form,
                                           const script::Object& rest,
                                           TypeEnv* type_env) {
    std::string state_name = form.as_pair()->car.as_symbol().c_str();
    Type* type = type_env->get_type();
    
    auto* s_env = new StateEnv(state_name, type_env, type, type_env);
    
    // Состояние не имеет тела, только обработчики
    // Обработчики будут добавлены позже через defmethod
    
    return new IR_StateValue(s_env);
}

// ============================================================================
// compile_static
// ============================================================================

StaticObject* FunctionCompiler::compile_static(const script::Object& form, Env* env) {
    (void)env;
    
    if (form.is_string()) {
        std::string str = form.to_std_string();
        return new StaticString(str);
    }
    
    if (form.is_number()) {
        if (form.is_float()) {
            return new StaticFloat(form.as_float());
        }
        double num = form.as_integer();
        return new StaticFloat(static_cast<float>(num));
    }
    
    return nullptr;
}

// ============================================================================
// compile_body — создает IR_Node для всех форм тела
// ============================================================================

void FunctionCompiler::compile_body(const script::Object& body_forms, FunctionEnv* env) {
    auto current = body_forms;
    IR_Value* last_val = nullptr;
    
    while (current.is_pair()) {
        last_val = compiler_->compile(current.as_pair()->car, env);
        current = current.as_pair()->cdr;
    }
    
    if (last_val) {
        env->emit(script::Object(), std::make_unique<IR_Return>(last_val));
    } else {
        Type* obj_type = ts_.lookup_type("object");
        env->emit(script::Object(), std::make_unique<IR_Return>(new IR_Const(obj_type, static_cast<s64>(0))));
    }
}

// ============================================================================
// parse_arguments
// ============================================================================

void FunctionCompiler::parse_arguments(const script::Object& args_form, FunctionEnv* env) {
    auto current = args_form;
    int arg_idx = env->params().size(); // учитываем уже существующие аргументы (например, this)
    
    while (current.is_pair()) {
        auto arg_decl = current.as_pair()->car;
        if (arg_decl.is_pair()) {
            std::string arg_name = arg_decl.as_pair()->car.as_symbol().c_str();
            std::string type_name = arg_decl.as_pair()->cdr.as_pair()->car.as_symbol().c_str();
            Type* arg_type = ts_.lookup_type(type_name);
            env->define_argument(arg_name, arg_type, arg_idx++);
        } else if (arg_decl.is_symbol()) {
            std::string arg_name = arg_decl.as_symbol().c_str();
            Type* arg_type = ts_.lookup_type("object");
            env->define_argument(arg_name, arg_type, arg_idx++);
        }
        current = current.as_pair()->cdr;
    }
}

// ============================================================================
// BUILD Phase (без изменений, но методы переименованы)
// ============================================================================

RelocatableBuffer FunctionCompiler::build(FunctionEnv* fe) {
    RelocatableBuffer code_buffer;
    RelocatableBuffer data_buffer;
    RelocatableBuffer debug_buffer;
    std::unordered_map<IR_Value*, u32> reg_map;
    
    const u32 ARG_START = 24; 
    const u32 LOCAL_START = 0;

    // 1. Регистрируем аргументы
    u32 next_arg = ARG_START;
    for (auto* arg_val : fe->params()) { 
        reg_map[arg_val] = next_arg++;
    }

    // 2. Регистрируем локальные переменные
    u32 next_local = LOCAL_START;
    for (auto& node : fe->code()) {
        for (auto* val : node->get_used_values()) {
            if (val && reg_map.find(val) == reg_map.end()) {
                if (next_local >= ARG_START) {
                    lg::error("Function {}: Out of local registers!", fe->name());
                }
                reg_map[val] = next_local++;
            }
        }
    }

    // 3. Генерируем байткод прямо в буфер
    for (auto& node : fe->code()) {
        node->generate(code_buffer, reg_map);  // ← generate теперь принимает RelocatableBuffer
    }

    // 4. Заголовок функции
    FunctionDesc desc{}; 
    desc.name = StringId(fe->name());
    desc.code_count = static_cast<u32>(code_buffer.size() / sizeof(Instruction));
    desc.code_ptr.offset = sizeof(FunctionDesc);
    desc.data_size = 0;
    desc.data_ptr.offset = 0;
    desc.debug_count = 0;
    desc.debug_ptr.offset = 0;

    // 5. Вставляем заголовок В НАЧАЛО буфера
    RelocatableBuffer result;
    
    std::string label_name = make_function_symbol(fe->name());
    std::string code_name = label_name + "#code";
    std::string data_name = label_name + "#data";
    std::string debug_name = label_name + "#debug";

    result.add_relocatable(offsetof(FunctionDesc, code_ptr), 
                           Relocation::Type::FIXED_ADDRESS, 
                           code_name);    
    result.add_relocatable(offsetof(FunctionDesc, data_ptr), 
                           Relocation::Type::FIXED_ADDRESS, 
                           data_name);
    result.add_relocatable(offsetof(FunctionDesc, debug_ptr), 
                           Relocation::Type::FIXED_ADDRESS, 
                           debug_name);
    result.add_bytes(&desc, sizeof(desc));

    // Добавляем сгенерированный код после заголовка
    result.add_label(code_name);
    result.add_buffer(code_buffer);

    result.add_label(data_name);
    result.add_buffer(data_buffer);

    result.add_label(debug_name);
    result.add_buffer(debug_buffer);

    return result;
}

RelocatableBuffer FunctionCompiler::build_method(MethodEnv* me) {

    std::unordered_map<IR_Value*, u32> reg_map;
    
    const u32 ARG_START = 24; 
    const u32 LOCAL_START = 0;

    u32 next_arg = ARG_START;
    for (auto* arg_val : me->params()) { 
        reg_map[arg_val] = next_arg++;
    }

    u32 next_local = LOCAL_START;
    for (auto& node : me->code()) {
        for (auto* val : node->get_used_values()) {
            if (val && reg_map.find(val) == reg_map.end()) {
                reg_map[val] = next_local++;
            }
        }
    }

    RelocatableBuffer code_buffer;  // буфер для кода
    RelocatableBuffer data_buffer;
    RelocatableBuffer debug_buffer;

    for (auto& node : me->code()) {
        node->generate(code_buffer, reg_map);
    }
    
    // Теперь создаем финальный буфер с заголовком
    RelocatableBuffer result;

    std::string symbol_name = make_method_symbol(me->type_env()->name(), me->name());
    std::string code_name = symbol_name + "#code";
    std::string data_name = symbol_name + "#data";
    std::string debug_name = symbol_name + "#debug";

    FunctionDesc desc{}; 
    desc.name = StringId(me->name());
    desc.code_count = static_cast<u32>(code_buffer.size() / sizeof(Instruction));
    desc.code_ptr.offset = sizeof(FunctionDesc);
    desc.data_size = 0;
    desc.data_ptr.offset = 0;
    desc.debug_count = 0;
    desc.debug_ptr.offset = 0;

    result.add_relocatable(offsetof(FunctionDesc, code_ptr), 
                           Relocation::Type::FIXED_ADDRESS, 
                           code_name);
    result.add_relocatable(offsetof(FunctionDesc, data_ptr), 
                           Relocation::Type::FIXED_ADDRESS, 
                           data_name);
    result.add_relocatable(offsetof(FunctionDesc, debug_ptr), 
                           Relocation::Type::FIXED_ADDRESS, 
                           debug_name);
    result.add_bytes(&desc, sizeof(desc));
    
    // Добавляем сгенерированный код
    result.add_label(code_name);
    result.add_buffer(code_buffer);
    
    result.add_label(data_name);
    result.add_buffer(data_buffer);

    result.add_label(debug_name);
    result.add_buffer(debug_buffer);
    return result;
}

RelocatableBuffer FunctionCompiler::build_state(StateEnv* se) {
    (void)se;
    RelocatableBuffer buffer;
    return buffer;
}

RelocatableBuffer FunctionCompiler::build_static(StaticObject* so) {
    RelocatableBuffer buffer;
    std::vector<u8> data;
    std::vector<u64> relocations;
    
    so->generate(data, relocations);
    
    buffer.add_bytes(data.data(), data.size());
    
    for (u64 pos : relocations) {
        buffer.add_relocatable(pos, Relocation::Type::FIXED_ADDRESS, "");
    }
    
    return buffer;
}

// ============================================================================
// Helpers
// ============================================================================

std::string FunctionCompiler::make_function_symbol(const std::string& name) {
    return fmt::format("{}-{}", name, s_lambda_index++);
}

std::string FunctionCompiler::make_method_symbol(const std::string& type_name, 
                                                  const std::string& method_name) {
    return type_name + "::" + method_name;
}

std::string FunctionCompiler::make_state_symbol(const std::string& type_name, 
                                                 const std::string& state_name) {
    return type_name + "::" + state_name;
}

} // namespace sootc