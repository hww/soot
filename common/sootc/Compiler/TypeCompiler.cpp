// common/sootc/Compiler/TypeCompiler.cpp
#include "TypeCompiler.hpp"
#include "common/carbon/files/TypeDesc.hpp"
#include "common/carbon/files/Definition.hpp"
#include "common/carbon/files/StateDesc.hpp"
#include "common/carbon/files/FunctionDesc.hpp"
#include "type_system/Deftype.hpp"
#include "type_system/TypeSystem.hpp"

namespace sootc {

TypeCompiler::TypeCompiler(TypeSystem& ts) : ts_(ts) {}

RelocatableBuffer TypeCompiler::compile_type(const script::Object& form, EnvironmentMap* constances) {

    auto& ts = TypeSystem::instance();
    auto resut = parse_deftype(form, &ts, constances);
    /**
     * The resukt is 
     *   struct DeftypeResult {
     *   TypeSpec  type;
     *   Type     *type_info = nullptr;
     *   TypeFlags flags;
     *   bool      create_runtime_type = true;
     * };
     */

    if (resut.flags.)
    // Парсим форму типа
    // (define-type MyClass (parent Object) 
    //   (methods ...)
    //   (states ...)
    //   (flags ...))
    
    RelocatableBuffer buffer;
    
    // Создаем TypeDesc
    TypeDesc type_desc;
    type_desc.name = StringId(type_name);
    type_desc.parent_type_id = StringId("object"); // по умолчанию
    type_desc.methods_count = 0;
    type_desc.states_count = 0;
    type_desc.flags = TypeFlags::None;
    type_desc.reg_class = RegClass::GPR_64;
    type_desc.load_size = 8;
    type_desc.in_memory_alignment = 8;
    type_desc.inline_array_stride_alignment = 8;
    type_desc.inline_array_start_alignment = 8;
    type_desc.offset = 0;
    
    // Парсим parent
    // ... 
    
    // Компилируем методы
    std::vector<MethodDef> methods;
    // std::vector<MethodDef> methods = compile_methods(methods_form);
    
    // Компилируем состояния
    std::vector<StateDef> states;
    // std::vector<StateDef> states = compile_states(states_form);
    
    type_desc.methods_count = methods.size();
    type_desc.states_count = states.size();
    
    // Собираем буфер
    return build_type_buffer(type_desc, methods, states);
}

RelocatableBuffer TypeCompiler::build_type_buffer(const TypeDesc& type_desc,
                                                   const std::vector<MethodDef>& methods,
                                                   const std::vector<StateDef>& states) {
    RelocatableBuffer buffer;
    
    // Записываем TypeDesc
    u32 type_start = buffer.size();
    buffer.add_bytes(&type_desc, sizeof(TypeDesc));
    
    // Отмечаем методы и состояния как relocatable
    u32 methods_ptr_offset = type_start + offsetof(TypeDesc, methods_offset);
    u32 states_ptr_offset = type_start + offsetof(TypeDesc, states_offset);
    
    if (!methods.empty()) {
        buffer.add_relocatable_offset(methods_ptr_offset);
    }
    if (!states.empty()) {
        buffer.add_relocatable_offset(states_ptr_offset);
    }
    
    // Записываем массив методов (MethodDef)
    u32 methods_start = 0;
    if (!methods.empty()) {
        methods_start = buffer.size();
        buffer.add_bytes(methods.data(), methods.size() * sizeof(MethodDef));
        
        // Обновляем methods_offset в TypeDesc
        Ptr<MethodDef>* methods_ptr = 
            reinterpret_cast<Ptr<MethodDef>*>(buffer.data() + methods_ptr_offset);
        methods_ptr->offset = methods_start;
    }
    
    // Записываем массив состояний (StateDef)
    u32 states_start = 0;
    if (!states.empty()) {
        states_start = buffer.size();
        buffer.add_bytes(states.data(), states.size() * sizeof(StateDef));
        
        // Обновляем states_offset в TypeDesc
        Ptr<StateDef>* states_ptr = 
            reinterpret_cast<Ptr<StateDef>*>(buffer.data() + states_ptr_offset);
        states_ptr->offset = states_start;
    }
    
    return buffer;
}

} // namespace sootc