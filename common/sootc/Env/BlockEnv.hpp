#pragma once

#include "common/sootc/Env/Env.hpp"
#include "common/type_system/TypeSpec.hpp" 

namespace sootc {

/*!
 * An Env for (let ...) statements.
 */
class BlockEnv : public Env {
public:
    BlockEnv(Env* parent, std::string name) 
        : Env(EnvKind::BLOCK_ENV, parent), m_name(name) {}
    
    // Поиск вложенного блока по имени
    BlockEnv* find_block(const std::string& name) override {
        if (m_name == name) return this;
        return m_parent ? m_parent->find_block(name) : nullptr;
    }
    
    // Локальные переменные блока
    void bind_local(const std::string& name, IR_Value* val) {
        m_locals[name] = val;
    }
    
    IR_Value* lookup_local(const std::string& name) {
        auto it = m_locals.find(name);
        return it != m_locals.end() ? it->second : nullptr;
    }
    
    // Для возврата из блока (block/return-from)
    Label end_label;           // метка выхода из блока
    IR_Reg* return_value;      // регистр для возвращаемого значения
    std::vector<TypeSpec> return_types;  // типы возврата
    
    const std::string& name() const { return m_name; }
    
    std::string print() const override {
        return fmt::format("BlockEnv({})", m_name);
    }
    
private:
    std::string m_name;
    std::unordered_map<std::string, IR_Value*> m_locals;
};

} // namespace sootc