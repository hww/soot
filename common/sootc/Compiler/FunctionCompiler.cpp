#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "common/carbon/files/FunctionDesc.hpp" 
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "sootc/IR/IR_Value.hpp"
#include "sootc/IR/IR_Node.hpp"
#include "sootc/Env/MethodEnv.hpp"
#include "sootc/Env/StateEnv.hpp"
#include "sootc/Env/TypeEnv.hpp"
#include "sootc/IR/StaticObject.hpp"

using namespace ::carbon::files;
using namespace ::carbon::vm;

namespace sootc {

FunctionCompiler::FunctionCompiler(TypeSystem& ts, Compiler* compiler)
    : ts_(ts), compiler_(compiler) {}

// ============================================================================
// DECLARE Phase
// ============================================================================

IR_Value* FunctionCompiler::declare_function(const script::Object& form, 
                                              const script::Object& rest, 
                                              Env* env) {
    (void)form;
    
    auto args_list = rest.as_pair()->car;
    auto body_forms = rest.as_pair()->cdr;

    auto* f_env = new FunctionEnv(env, "lambda");
    f_env->set_source_form(body_forms);
    
    parse_arguments(args_list, f_env);
    
    return new IR_FunctionValue(f_env);
}

IR_Value* FunctionCompiler::declare_method(const script::Object& form,
                                            const script::Object& rest,
                                            TypeEnv* type_env,
                                            int method_id) {
    (void)form;
    
    auto args_list = rest.as_pair()->car;
    auto body_forms = rest.as_pair()->cdr;
    
    std::string method_name = form.as_pair()->car.as_symbol().c_str();
    Type* type = type_env->get_type();
    
    auto* m_env = new MethodEnv(method_name, type_env, type, type_env);
    m_env->method_id = method_id;
    m_env->set_source_form(body_forms);
    
    // Парсим аргументы (this уже добавлен в конструкторе)
    parse_arguments(args_list, m_env);
    
    return new IR_MethodValue(m_env);
}

IR_Value* FunctionCompiler::declare_state(const script::Object& form,
                                           const script::Object& rest,
                                           TypeEnv* type_env) {
    (void)rest;
    
    std::string state_name = form.as_pair()->car.as_symbol().c_str();
    Type* type = type_env->get_type();
    
    auto* s_env = new StateEnv(state_name, type_env, type, type_env);
    s_env->set_source_form(rest);
    
    return new IR_StateValue(s_env);
}

StaticObject* FunctionCompiler::declare_static(const script::Object& form, Env* env) {
    (void)env;
    
    if (form.is_string()) {
        std::string str = form.to_std_string();
        return new StaticString(str);
    }
    
    if (form.is_number()) {
        if (form.is_float()) {
            return new StaticFloat(form.as_float());
        }
        // Integer
        double num = form.as_integer();
        return new StaticFloat(static_cast<float>(num));
    }
    
    // TODO: pairs, structures, etc.
    return nullptr;
}

// ============================================================================
// RESOLVE Phase
// ============================================================================

void FunctionCompiler::resolve_body(FunctionEnv* f_env) {
    auto body_forms = f_env->source_form();
    auto current = body_forms;
    
    IR_Value* last_val = nullptr;
    while (current.is_pair()) {
        last_val = compiler_->resolve(current.as_pair()->car, f_env);
        current = current.as_pair()->cdr;
    }
    
    if (last_val) {
        f_env->emit(script::Object(), std::make_unique<IR_Return>(last_val));
    } else {
        Type* obj_type = ts_.lookup_type("object");
        f_env->emit(script::Object(), std::make_unique<IR_Return>(new IR_Const(obj_type, static_cast<s64>(0))));
    }
}

void FunctionCompiler::resolve_method_body(MethodEnv* m_env) {
    // Аналогично resolve_body, но с MethodEnv
    auto body_forms = m_env->source_form();
    auto current = body_forms;
    
    IR_Value* last_val = nullptr;
    while (current.is_pair()) {
        last_val = compiler_->resolve(current.as_pair()->car, m_env);
        current = current.as_pair()->cdr;
    }
    
    if (last_val) {
        m_env->emit(script::Object(), std::make_unique<IR_Return>(last_val));
    }
}

void FunctionCompiler::resolve_state_body(StateEnv* s_env) {
    // Состояние не имеет своего тела, оно только содержит методы-обработчики
    // Обработчики уже добавлены через bind в TypeEnv
}

// ============================================================================
// BUILD Phase
// ============================================================================

RelocatableBuffer FunctionCompiler::build(FunctionEnv* fe) {
    FunctionDescBuilder func_builder; 
    std::unordered_map<IR_Value*, u32> reg_map;
    
    const u32 ARG_START = 24; 
    const u32 LOCAL_START = 0;

    u32 next_arg = ARG_START;
    for (auto* arg_val : fe->params()) { 
        reg_map[arg_val] = next_arg++;
    }

    u32 next_local = LOCAL_START;
    for (auto& node : fe->nodes()) {
        for (auto* val : node->get_used_values()) {
            if (val && reg_map.find(val) == reg_map.end()) {
                if (next_local >= ARG_START) {
                    lg::error("Function {}: Out of local registers!", fe->name());
                }
                reg_map[val] = next_local++;
            }
        }
    }

    for (auto& node : fe->nodes()) {
        node->generate(func_builder, reg_map);
    }
    
    auto instructions = func_builder.get_instructions();

    FunctionDesc desc{}; 
    desc.code_count = static_cast<u32>(instructions.size());
    desc.code_ptr = reinterpret_cast<Instruction*>(sizeof(FunctionDesc));

    RelocatableBuffer buffer;  
    buffer.add_bytes(&desc, sizeof(desc));
    
    std::string symbol_name = make_function_symbol(fe->name());
    buffer.add_relocatable(offsetof(FunctionDesc, code_ptr), 
                           Relocation::Type::FILE_RELATIVE, 
                           symbol_name + "#code");

    if (!instructions.empty()) {
        buffer.add_bytes(instructions.data(), instructions.size() * sizeof(Instruction));
    }
    
    return buffer;
}

RelocatableBuffer FunctionCompiler::build_method(MethodEnv* me) {
    // Аналогично build, но с MethodEnv
    FunctionDescBuilder func_builder; 
    std::unordered_map<IR_Value*, u32> reg_map;
    
    const u32 ARG_START = 24; 
    const u32 LOCAL_START = 0;

    u32 next_arg = ARG_START;
    for (auto* arg_val : me->params()) { 
        reg_map[arg_val] = next_arg++;
    }

    u32 next_local = LOCAL_START;
    for (auto& node : me->nodes()) {
        for (auto* val : node->get_used_values()) {
            if (val && reg_map.find(val) == reg_map.end()) {
                reg_map[val] = next_local++;
            }
        }
    }

    for (auto& node : me->nodes()) {
        node->generate(func_builder, reg_map);
    }
    
    auto instructions = func_builder.get_instructions();
    RelocatableBuffer buffer;

    FunctionDesc desc{}; 
    desc.code_count = static_cast<u32>(instructions.size());
    desc.code_ptr = reinterpret_cast<Instruction*>(sizeof(FunctionDesc));

    buffer.add_bytes(&desc, sizeof(desc));
    
    std::string symbol_name = make_method_symbol(me->type_env()->name(), me->name());
    buffer.add_relocatable(offsetof(FunctionDesc, code_ptr), 
                           Relocation::Type::FILE_RELATIVE, 
                           symbol_name + "#code");

    if (!instructions.empty()) {
        buffer.add_bytes(instructions.data(), instructions.size() * sizeof(Instruction));
    }
    
    return buffer;
}

RelocatableBuffer FunctionCompiler::build_state(StateEnv* se) {
    // State не содержит кода, только метаданные
    // Возвращает пустой буфер или буфер с информацией о состоянии
    RelocatableBuffer buffer;
    
    // Можно записать информацию о состоянии для отладки
    // Но сам state не генерирует код
    
    return buffer;
}

RelocatableBuffer FunctionCompiler::build_static(StaticObject* so) {
    RelocatableBuffer buffer;
    std::vector<u8> data;      // ← отдельный не-const вектор
    std::vector<u64> relocations;
    
    so->generate(data, relocations);
    
    // Добавляем байты в буфер
    buffer.add_bytes(data.data(), data.size());
    
    // Добавляем релокации
    for (u64 pos : relocations) {
        buffer.add_relocatable(pos, Relocation::Type::FILE_RELATIVE, "");
    }
    
    return buffer;
}

// ============================================================================
// Helpers
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

void FunctionCompiler::compile_body_from_forms(const script::Object& body_forms, FunctionEnv* env) {
    auto current = body_forms;
    while (current.is_pair()) {
        compiler_->resolve(current.as_pair()->car, env);
        current = current.as_pair()->cdr;
    }
}

std::string FunctionCompiler::make_function_symbol(const std::string& name) {
    return name;
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