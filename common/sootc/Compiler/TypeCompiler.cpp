#include "sootc/Compiler/TypeCompiler.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "sootc/Compiler/Env.hpp"
#include "sootc/IR/IR_Value.hpp"
#include "common/type_system/Deftype.hpp"
#include "common/util/Log.hpp"

namespace sootc {

using namespace ::carbon::files;

TypeCompiler::TypeCompiler(TypeSystem& ts, Compiler* compiler) 
    : ts_(ts), compiler_(compiler) {}

IR_Value* TypeCompiler::declare(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;

    // 1. Парсим структуру через Deftype (наполняем метаданные в TypeSystem)
    DeftypeResult result = parse_deftype(rest, &ts_, nullptr);
    Type* type_info = result.type_info;
    
    if (!type_info) {
        lg::error("Failed to parse type definition");
        return nullptr;
    }

    // 2. Создаем TypeEnv — контекст для этого типа
    auto* t_env = new TypeEnv(type_info);

    // 3. Наполняем методы (декларативно)
    for (auto& method : type_info->get_methods_defined_for_type()) {
        // Создаем в куче, чтобы объект жил до конца компиляции типа
        auto* m_env = new MethodEnv(t_env, method.name, type_info);
        
        // Если у метода уже есть сырые формы (тело), сохраняем их для build
        // m_env->set_source_forms(...); 
        
        t_env->add_method(*m_env);
    }

    // 4. Наполняем состояния (StateEnv напрямую)
    for (const auto& [s_name, s_type_spec] : type_info->get_states_declared_for_type()) {
        // Аналогично: создаем StateEnv в куче
        auto* s_env = new StateEnv(t_env, s_name, type_info);
        
        // Здесь можно сразу прокинуть спецификацию типа состояния
        // s_env->set_type_spec(s_type_spec);

        t_env->add_state(*s_env);
    }
    // 5. Регистрируем в Env и возвращаем IR_Type
    auto* ir_type = new IR_Type(t_env);
    env->bind(type_info->get_name(), ir_type);
    
    return ir_type;
}

RelocatableBuffer TypeCompiler::build(IR_Type* ir_type) {
    RelocatableBuffer buffer;
    auto* t_env = ir_type->get_env();
    
    // Нам нужно полное определение TypeDesc для sizeof
    ::carbon::files::TypeDesc desc{};
    // Заполняем поля desc из t_env->get_type_info()...
    
    u32 type_start = buffer.size();
    buffer.add_bytes(&desc, sizeof(::carbon::files::TypeDesc));
    
    // Релокации для методов
    auto methods = t_env->methods();
    if (!methods.empty()) {
        u32 m_offset = buffer.size();
        buffer.add_bytes(methods.data(), methods.size() * sizeof(MethodDef));
        
        u32 ptr_field = type_start + offsetof(::carbon::files::TypeDesc, methods_offset);
        // Фиксим смещение в уже записанном TypeDesc
        if (buffer.size() >= ptr_field + sizeof(u64)) {
            *reinterpret_cast<u64*>(buffer.data() + ptr_field) = (u64)m_offset;
        }
        buffer.add_relocatable_offset(ptr_field);
    }
    
    // Релокации для состояний (StateDef)
    auto states = t_env->states();
    if (!states.empty()) {
        u32 s_offset = buffer.size();
        buffer.add_bytes(states.data(), states.size() * sizeof(StateDef));
        
        u32 ptr_field = type_start + offsetof(::carbon::files::TypeDesc, states_offset);
        if (buffer.size() >= ptr_field + sizeof(u64)) {
            *reinterpret_cast<u64*>(buffer.data() + ptr_field) = (u64)s_offset;
        }
        buffer.add_relocatable_offset(ptr_field);
    }
    
    return buffer;
}

} // namespace sootc