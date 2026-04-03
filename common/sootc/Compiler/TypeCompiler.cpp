#include "sootc/Compiler/TypeCompiler.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/IR/IR_Value.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "common/type_system/Deftype.hpp"
#include "common/util/Log.hpp"
#include "type_system/Type.hpp"
#include "type_system/TypeSpec.hpp"
#include <cstddef>

namespace sootc {

using namespace ::carbon::files;

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
             type_info->get_name(), type_info->get_parent(), 
             type_info->get_methods_count(), type_info->states_count());
    // Создаем TypeEnv
    auto* t_env = new TypeEnv(type_info->get_name(), type_info, env);

    // Создаем MethodEnv для каждого объявленного метода
    for (u32 id = 0; id < type_info->get_methods_count(); ++id) {
        MethodInfo method;
        if (ts_.try_lookup_method(type_info->get_name(), id, &method)) {
            auto* m_env = new MethodEnv(id, method.name, t_env, type_info);
            // Метод пока без тела — тело появится позже в defmethod
            t_env->bind(method.name, new IR_MethodValue(m_env));
        } else {
            lg::error("Method ID {} not found in type '{}'", id, type_info->get_name());
        }
    }

    // Создаем StateEnv для каждого объявленного состояния
    for (const auto& [s_name, s_type_spec] : type_info->get_states_declared_for_type()) {
        auto* s_env = new StateEnv(s_name, t_env, type_info, t_env);
        t_env->bind(s_name, new IR_StateValue(s_env));
    }
    
    // Регистрируем тип в окружении
    auto* ir_type = new IR_Type(t_env);
    env->bind(type_info->get_name(), ir_type);
    
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
    std::string parent_name = start_env->get_type()->get_parent();
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
    RelocatableBuffer buffer;
    Type* type_info = t_env->get_type();
    std::string type_name = type_info->get_name();

    // 1. Заголовок TypeDesc
    TypeDesc desc{};
    desc.name = StringId(type_name.c_str()); 
    desc.parent_type_id = StringId(type_info->get_parent().c_str());
    desc.size_in_memory = type_info->get_size_in_memory();
    desc.methods_count  = type_info->get_methods_count();
    desc.states_count   = type_info->states_count();
    desc.flags = 0;
    desc.methods_offset = Ptr<MethodDef>();
    desc.states_offset = Ptr<StateDef>();

    u32 type_start = buffer.size();
    buffer.add_bytes(&desc, sizeof(TypeDesc));
    
    u32 methods_offset_field = type_start + offsetof(TypeDesc, methods_offset);
    u32 states_offset_field = type_start + offsetof(TypeDesc, states_offset);
    
    // ========================================================================
    // 2. VTable (методы)
    // ========================================================================
    if (desc.methods_count > 0) {
        for (int id = 0; id < (int)desc.methods_count; ++id) {
            TypeEnv* defining_type = nullptr;
            MethodEnv* m_env = find_method_in_hierarchy(t_env, id, defining_type);
            
            if (!m_env) {
                    MethodInfo info;
                if (type_info->get_my_method(id, &info)) {
                    throw std::runtime_error(fmt::format("Method '{}' not found in type '{}'", info.name, type_name));
                } else {
                    throw std::runtime_error(fmt::format("Method ID {} not found in type '{}'", id, type_name));
                }
            }
            
            std::string owner_name = defining_type->get_type()->get_name();
            std::string method_symbol = owner_name + "::" + m_env->name();
            
            buffer.add_u64(0);
            buffer.add_relocatable(buffer.size() - 8, 
                                   Relocation::Type::FIXED_ADDRESS, 
                                   method_symbol + "#code");
        }
        
        buffer.add_relocatable(methods_offset_field, 
                               Relocation::Type::FIXED_ADDRESS, 
                               type_name + "#methods");
    }
    
    // ========================================================================
    // 3. StateTable (состояния)
    // ========================================================================
    if (desc.states_count > 0) {
        for (auto* s_env : t_env->states()) {
            u32 slot_pos = buffer.size();
            StateDef slot{};
            slot.name = StringId(s_env->name().c_str());
            slot.flags = SymbolFlags::None;
            slot.ptr = Ptr<u8>();
            buffer.add_bytes(&slot, sizeof(StateDef));
            
            std::string state_label = type_name + "::" + s_env->name();
            u32 ptr_in_slot = slot_pos + offsetof(StateDef, ptr);
            buffer.add_relocatable(ptr_in_slot, Relocation::Type::FIXED_ADDRESS, state_label);
        }
        
        buffer.add_relocatable(states_offset_field, 
                               Relocation::Type::FIXED_ADDRESS, 
                               type_name + "#states");
    }
    
    return buffer;
}

} // namespace sootc