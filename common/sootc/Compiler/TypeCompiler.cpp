#include "sootc/Compiler/TypeCompiler.hpp"
#include "files/FunctionDesc.hpp"
#include "files/StateDesc.hpp"
#include "lib/Ptr.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Compiler/MethodCompiler.hpp"
#include "sootc/Compiler/StateCompiler.hpp"
#include "sootc/IR/IR_Value.hpp"
#include "carbon/files/RelocatableBuffer.hpp"
#include "carbon/lib/Variant.hpp"
#include "type_system/Deftype.hpp"
#include "util/Log.hpp"
#include "type_system/Type.hpp"
#include "type_system/TypeSpec.hpp"
#include <cstddef>

namespace sootc {

using namespace ::carbon::files;
using namespace ::carbon::lib;

TypeCompiler::TypeCompiler(TypeSystem& ts, Compiler* compiler) 
    : ts_(ts), compiler_(compiler) {}

// ============================================================================
// compile — один проход
// ============================================================================

IR_Value* TypeCompiler::compile(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;

    // Парсим определение типа
    DeftypeResult result = parse_deftype(rest, &ts_, nullptr);
    Type* type_info = result.type_info;
    
    if (!type_info) {
        lg::error("Failed to parse type definition");
        return nullptr;
    }
    lg::info("TypeCompiler compile type '{}', parent '{}', methods {}, states {}", 
             type_info->name(), type_info->parent(), 
             type_info->get_methods_count(), type_info->states_count());

    // Создаем TypeEnv
    auto* t_env = new TypeEnv(type_info->name(), type_info, env);

    // Создаем MethodEnv для каждого объявленного метода
    for (u32 id = 0; id < type_info->get_methods_count(); ++id) {
        MethodInfo method;
        if (ts_.try_lookup_method(type_info->name(), id, &method)) {
            auto* m_env = new MethodEnv(id, method.name, t_env, type_info);
            // Метод пока без тела — тело появится позже в defmethod
            t_env->bind(method.name, new IR_MethodValue(m_env));
        } else {
            lg::error("Method ID {} not found in type '{}'", id, type_info->name());
        }
    }

    // Создаем StateEnv для каждого объявленного состояния
    for (const auto& [s_name, s_type_spec] : type_info->get_states_declared_for_type()) {
        auto* s_env = new StateEnv(s_name, t_env, type_info, t_env);
        t_env->bind(s_name, new IR_StateValue(s_env));
    }
    
    // Регистрируем тип в окружении
    auto* ir_type = new IR_Type(t_env);
    env->bind(type_info->name(), ir_type);
    
    return ir_type;
}

// ============================================================================
// BUILD Phase
// ============================================================================

MethodEnv* TypeCompiler::find_method_in_hierarchy(TypeEnv* start_env, int method_id, TypeEnv*& out_defining_type) {
    // Проверяем текущий тип
    MethodEnv* m_env = start_env->get_method(method_id);
    if (m_env) {
        out_defining_type = start_env;
        return m_env;
    }
    
    // Ищем в родителях через глобальное окружение
    std::string parent_name = start_env->get_type()->parent();
    if (parent_name.empty() || parent_name == "object") {
        out_defining_type = nullptr;
        return nullptr;
    }
    
    Env* global = start_env->global_env();
    IR_Value* parent_val = global->lookup(parent_name);
    if (!parent_val) {
        out_defining_type = nullptr;
        return nullptr;
    }
    
    auto* parent_ir_type = dynamic_cast<IR_Type*>(parent_val);
    if (!parent_ir_type) {
        out_defining_type = nullptr;
        return nullptr;
    }
    
    // Рекурсивно ищем в родителе
    return find_method_in_hierarchy(parent_ir_type->get_env(), method_id, out_defining_type);
}

RelocatableBuffer TypeCompiler::build(TypeEnv* t_env) {
    Type* type = t_env->get_type();
    std::string type_name = type->name();

    RelocatableBuffer result_desc(type_name + "#descriptor", "type", true);
    RelocatableBuffer result_vtable(type_name + "#vtable", "vtable", true);
    RelocatableBuffer result_stable(type_name + "#stable", "stable", true);
    RelocatableBuffer result_functions(type_name+"::vtable::functions-segment", "void", true);
    RelocatableBuffer result_states(type_name+"::vtable::states-segment", "void", true);

    // 1. Заголовок TypeDesc
    TypeDesc desc{};
    desc.name = StringId(type_name.c_str()); 
    desc.parent_type_id = StringId(type->parent().c_str());
    desc.size_in_memory = type->get_size_in_memory();
    desc.methods_count  = type->get_methods_count();
    desc.states_count   = type->states_count();
    desc.methods_ptr = nullptr;
    desc.states_ptr = nullptr;
    desc.flags = 0;



    // make references to methods only if there is vtable
    result_desc.add_relocatable(offsetof(TypeDesc, methods_ptr),
                                desc.methods_count ? Relocation::Type::LABEL_ADDRESS : Relocation::Type::NULL_ADDRESS, 
                                result_vtable.name());

    // make references to states only if there is states
    result_desc.add_relocatable(offsetof(TypeDesc, states_ptr), 
                            desc.states_count ? Relocation::Type::LABEL_ADDRESS : Relocation::Type::NULL_ADDRESS, 
                            result_stable.name());
    
    // write header
    result_desc.add_bytes(&desc, sizeof(TypeDesc));

    // ========================================================================
    // 2. VTable (методы)
    // ========================================================================

    // N.B. Важно ответить что на этапе compile все проверки должны пройти
    //      тоесть если метод не определен то ошибка уже сформирована

    if (desc.methods_count > 0) {

        for (int id = 0; id < (int)desc.methods_count; ++id) {
            
            // Method Step 1. Найти окружение метода
            TypeEnv* method_type = nullptr;
            MethodEnv* method_env = find_method_in_hierarchy(t_env, id, method_type);

            // Защита от ошибок
            if (!method_env) {
                MethodInfo info;
                if (type->get_my_method(id, &info)) {
                    throw std::runtime_error(fmt::format("Method '{}' not found in type '{}'", info.name, type_name));
                } else {
                    throw std::runtime_error(fmt::format("Method ID {} not found in type '{}'", id, type_name));
                }
            }

            // Method Step 2. получить тип метода это может быть родительский тип
            std::string type_name = method_type->get_type()->name();
            // получить метку метода
            std::string method_label = type_name + "::" + method_env->name() + "#descriptor";
            
            // Method Step 3. Вставляем заголовок FunctionDesc с нулевыми полями (потенциально с именем метода для отладки)
            result_vtable.add_relocatable(
                offsetof(Ptr<FunctionDesc>, ptr), 
                Relocation::Type::LABEL_ADDRESS, 
                method_label);
            Ptr<FunctionDesc> empty_pointer;
            result_vtable.add_bytes(&empty_pointer, sizeof(Ptr<FunctionDesc>)); // function pointer (будет заполнено позже)

            if (type == method_env->type()) {
                // Method Step 3. Этот метод этого класса создадим его имплементацию
                MethodCompiler method_compiler(ts_, compiler_);
                auto method_desc = method_compiler.build(method_env);
                result_functions.add_buffer(method_desc);
            }
        }
    }
    
    // ========================================================================
    // 3. StateTable (состояния)
    // ========================================================================
    
    if (desc.states_count > 0) {

        for (auto* s_env : t_env->states()) {
            std::string state_label = type_name + "::" + s_env->name() + "#descriptor";

            result_stable.add_relocatable(
                offsetof(Ptr<FunctionDesc>, ptr), 
                Relocation::Type::LABEL_ADDRESS, 
                state_label);
            Ptr<FunctionDesc> empty_pointer;
            result_stable.add_bytes(&empty_pointer, sizeof(Ptr<StateDesc>));  // state pointer (будет заполнено позже)

            if (type == s_env->type()) {
                // Method Step 3. Этот метод этого класса создадим его имплементацию
                StateCompiler state_compiler(ts_, compiler_);
                auto state_desc = state_compiler.build(s_env);
                result_functions.add_buffer(state_desc);
            }            
        }
    }


    result_desc.add_buffer(result_vtable);
    result_desc.add_buffer(result_stable);
    result_desc.add_buffer(result_functions);
    result_desc.add_buffer(result_states);

    return result_desc;
}

} // namespace sootc