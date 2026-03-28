#pragma once

#include "common/carbon/files/TypeDesc.hpp"
#include "common/carbon/files/Definition.hpp"
#include "common/carbon/files/MethodDef.hpp"
#include "common/carbon/files/StateDef.hpp"
#include <vector>
#include <memory>

namespace carbon::files {

class TypeBuilder {
public:
    TypeBuilder(const std::string& name, const std::string& parent);
    
    // Установка свойств
    void set_flags(TypeFlags flags);
    void set_reg_class(RegClass reg_class);
    void set_load_size(int size);
    void set_in_memory_alignment(int alignment);
    void set_offset(int offset);
    
    // Добавление методов
    void add_method(const std::string& name, std::vector<Instruction> code, 
                    MethodFlags flags = MethodFlags::None);
    void add_method(const std::string& name, FunctionDesc* function);
    
    // Добавление состояний
    void add_state(const std::string& name, const std::string& parent,
                   StateFlags flags = StateFlags::None);
    void add_state(const std::string& name, StateDesc* state);
    
    // Построение
    std::vector<u8> build();
    
    // Получение результата
    TypeDesc* get_type_desc() { return type_desc_.get(); }
    
private:
    std::unique_ptr<TypeDesc> type_desc_;
    std::vector<MethodDef> methods_;
    std::vector<StateDef> states_;
    std::vector<std::vector<u8>> method_data_;  // сериализованные функции
    std::vector<std::vector<u8>> state_data_;   // сериализованные состояния
};

} // namespace carbon::files