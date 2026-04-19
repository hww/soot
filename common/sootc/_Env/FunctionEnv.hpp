#pragma once

#include "common/sootc/Env/DeclareEnv.hpp"
#include "common/type_system/TypeSpec.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace sootc {

class IR_Reg;
class IR_Value;

/*!
 * FunctionEnv - только окружение функции
 * Отвечает за: переменные, параметры, регистры
 * НЕ отвечает за: генерацию кода, инструкции, бинарники
 */
class FunctionEnv : public DeclareEnv {
public:
    FunctionEnv(Env* parent, const std::string& name);
    ~FunctionEnv() = default;

    // ========================================================================
    // Env interface
    // ========================================================================
    std::string print() const override;
    IR_Value* lookup(const std::string& name) override;
    void bind(const std::string& name, IR_Value* val) override;
    std::string get_value_name(IR_Value* value) const override;

    // ========================================================================
    // Register management (только аллокация, не генерация!)
    // ========================================================================
    IR_Reg* alloc_reg(Type* type);
    const std::vector<std::unique_ptr<IR_Reg>>& reg_vals() const { return m_iregs; }
    u8 get_reg_index(IR_Reg* reg) const;
    
    // ========================================================================
    // Parameters
    // ========================================================================
    void define_argument(const std::string& name, Type* type, int reg_index);
    const std::vector<IR_Reg*>& params() const { return m_params; }
    IR_Reg* lookup_param(const std::string& name) const;
    int get_max_param_index() const;

    // ========================================================================
    // Getters
    // ========================================================================
    const std::string& get_name() const { return m_name; }
    void set_name(const std::string& name) { m_name = name; }
    const std::vector<IR_Value*>& symbols() const override { return m_ordered_symbols; }
    const std::unordered_map<std::string, IR_Value*>& symbols_map() const { return m_symbols_map; }
    
    // ========================================================================
    // Return type
    // ========================================================================
    void set_return_type(Type* type) { m_return_type = type; }
    Type* get_return_type() const { return m_return_type; }

    // ========================================================================
    // Memory management
    // ========================================================================
    template <typename T, class... Args>
    T* alloc_val(Args&&... args) {
        std::unique_ptr<T> new_obj = std::make_unique<T>(std::forward<Args>(args)...);
        m_vals.push_back(std::move(new_obj));
        return static_cast<T*>(m_vals.back().get());
    }
    // ========================================================================
    // Новые методы для генерации кода
    // ========================================================================     
    void add_instruction(Opcode op, u8 dest, u8 src1, u8 src2);
    void add_instruction_imm_u16(Opcode op, u8 dest, u16 imm);
    void add_instruction_imm_s16(Opcode op, u8 dest, i16 imm);
    void add_label(const std::string& name);
    void add_branch_reference(const std::string& label_name);
    
    void set_reg(IR_Value* val, u8 reg);
    u8 get_reg_index(IR_Value* val) const;
    u8 alloc_reg();
    
    // Хранилище инструкций
    std::vector<Instruction>& instructions() { return m_instructions; }   
private:
    std::string m_name;
    Type* m_return_type = nullptr;
    
    // Регистры (только хранение, без инструкций!)
    std::vector<std::unique_ptr<IR_Reg>> m_iregs;
    int m_next_reg = 0;
    
    // Параметры
    std::vector<IR_Reg*> m_params;
    std::unordered_map<std::string, IR_Reg*> m_param_map;
    
    // Symbol table
    std::unordered_map<std::string, IR_Value*> m_symbols_map;
    std::vector<IR_Value*> m_ordered_symbols;
    
    // Вспомогательные хранилища
    std::vector<std::unique_ptr<IR_Value>> m_vals;

    // Генерация кода
    std::vector<Instruction> m_instructions;
    std::unordered_map<IR_Value*, u8> m_reg_map;
    std::vector<std::string> m_branch_references;
};

} // namespace sootc