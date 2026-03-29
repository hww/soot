// TypeCompiler.cpp
#include "TypeCompiler.hpp"
#include "common/type_system/Deftype.hpp"
#include "common/carbon/lib/Ptr.hpp"
#include "common/util/Log.hpp"

namespace sootc {

TypeCompiler::TypeCompiler(TypeSystem& ts) : ts_(ts) {}

RelocatableBuffer TypeCompiler::compile(const script::Object& form, Env* env) {
    // Парсим deftype
    DeftypeResult result = parse_deftype(form, &ts_, nullptr);
    
    // Получаем информацию о типе
    StructureType* struct_type = dynamic_cast<StructureType*>(result.type_info);
    if (!struct_type) {
        lg::error("Only structure types are supported for serialization");
        return {};
    }
    
    // Создаем TypeDesc
    TypeDesc type_desc;
    type_desc.name = StringId(struct_type->get_name());
    type_desc.parent_type_id = StringId(struct_type->get_parent());
    type_desc.methods_count = struct_type->get_num_methods();
    type_desc.states_count = struct_type->get_states_declared_for_type().size();
    type_desc.flags = result.flags.flag;
    type_desc.load_size = struct_type->get_load_size();
    type_desc.in_memory_alignment = struct_type->get_in_memory_alignment();
    type_desc.inline_array_stride_alignment = struct_type->get_inline_array_stride_alignment();
    type_desc.inline_array_start_alignment = struct_type->get_inline_array_start_alignment();
    type_desc.offset = struct_type->get_offset();
    
    // Собираем методы
    std::vector<MethodDef> methods;
    for (const auto& method : struct_type->get_methods_defined_for_type()) {
        MethodDef method_def;
        method_def.name = StringId(method.name);
        method_def.type = StringId("function");
        method_def.flags = method.no_virtual ? MethodFlags::None : MethodFlags::Virtual;
        method_def.data = nullptr;
        methods.push_back(method_def);
    }
    
    // Собираем состояния
    std::vector<StateDef> states;
    for (const auto& [state_name, state_type] : struct_type->get_states_declared_for_type()) {
        StateDef state_def;
        state_def.name = StringId(state_name);
        state_def.type = StringId("state");
        state_def.flags = SymbolFlags::Export;
        state_def.data = nullptr;
        states.push_back(state_def);
    }
    
    // Строим буфер
    return build_type_buffer(type_desc, methods, states);
}

RelocatableBuffer TypeCompiler::build_type_buffer(const TypeDesc& type_desc,
                                                   const std::vector<MethodDef>& methods,
                                                   const std::vector<StateDef>& states) {
    RelocatableBuffer buffer;
    
    // Записываем TypeDesc
    u32 type_start = buffer.size();
    buffer.add_bytes(&type_desc, sizeof(TypeDesc));
    
    // Отмечаем pointers как relocatable
    u32 methods_ptr_offset = type_start + offsetof(TypeDesc, methods_offset);
    u32 states_ptr_offset = type_start + offsetof(TypeDesc, states_offset);
    
    if (!methods.empty()) {
        buffer.add_relocatable_offset(methods_ptr_offset);
    }
    if (!states.empty()) {
        buffer.add_relocatable_offset(states_ptr_offset);
    }
    
    // Записываем массив методов
    u32 methods_start = 0;
    if (!methods.empty()) {
        methods_start = buffer.size();
        buffer.add_bytes(methods.data(), methods.size() * sizeof(MethodDef));
        
        // Обновляем methods_offset в TypeDesc
        Ptr<MethodDef>* methods_ptr = 
            reinterpret_cast<Ptr<MethodDef>*>(buffer.data() + methods_ptr_offset);
        methods_ptr->offset = methods_start;
    }
    
    // Записываем массив состояний
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