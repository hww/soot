#pragma once

#include "Node.hpp"
#include "Parameter.hpp"
#include "LocalVariable.hpp"
#include "file/ProgramBinaryElement.hpp"
#include "carbon/vm/Instructions.hpp"
#include "sooti/Object.hpp"
#include "type_system/Type.hpp"
#include <vector>
#include <unordered_map>
#include <memory>

using namespace carbon;

namespace sootc {

class ExpressionNode;

class FunctionNode : public Node {
    std::string m_name;
    
    // Параметры
    std::vector<Parameter*> m_params;
    std::unordered_map<std::string, u8> m_param_map;
    
    // Локальные переменные
    std::vector<std::unique_ptr<LocalVariable>> m_locals;
    int m_next_reg = 0;
    
    // Результаты выражений
    std::unordered_map<const Node*, u8> m_reg_map;
    
    // Генерация кода
    std::vector<Instruction> m_instructions;
    std::vector<u64> m_constants;
    std::vector<u8> m_constants_kind;
    
    // Branch resolution
    std::unordered_map<std::string, u32> m_labels;
    std::vector<std::pair<std::string, u32>> m_unresolved_branches;
    
    // Тело функции
    std::unique_ptr<ExpressionNode> m_body;
    
protected:
    void update_self_cache() override {
        m_cached_function = this;
    }
    
public:
    explicit FunctionNode(const std::string& name);
    ~FunctionNode() = default;  // деструктор в .cpp не нужен, т.к. unique_ptr сам удалит
    
    const char* node_type() const override { return "FunctionNode"; }
    std::string to_string() const override;
    
    // ========================================================================
    // Имя
    // ========================================================================
    const std::string& name() const { return m_name; }
    
    // ========================================================================
    // Параметры
    // ========================================================================
    void add_parameter(const std::string& name, Type* type);
    
    // ========================================================================
    // Регистры
    // ========================================================================
    u8 alloc_reg(Type* type);
    u8 get_reg(const Node* node) const;
    void set_reg(const Node* node, u8 reg);
    
    // ========================================================================
    // Константы
    // ========================================================================
    enum class ConstKind { INT, FLOAT, STRING };
    u16 add_constant(u64 value, ConstKind kind = ConstKind::INT);
    
    // ========================================================================
    // Инструкции
    // ========================================================================
    void add_instruction(Opcode op, u8 dest, u8 src1 = 0, u8 src2 = 0);
    void add_instruction_imm_u16(Opcode op, u8 dest, u16 imm);
    
    // ========================================================================
    // Метки
    // ========================================================================
    void add_label(const std::string& name);
    void add_branch_reference(const std::string& label);
    void resolve_branches();
    
    // ========================================================================
    // Генерация кода
    // ========================================================================
    void set_body(std::unique_ptr<ExpressionNode> body);
    void emit_body(ExpressionNode* body);
    void emit_body();
    
    // ========================================================================
    // Сериализация
    // ========================================================================
    carbon::ProgramBinaryElement build_binary(const std::string& module_name);
    
    // ========================================================================
    // Доступ к результатам
    // ========================================================================
    const std::vector<Instruction>& instructions() const { return m_instructions; }
    const std::vector<u64>& constants() const { return m_constants; }
};

} // namespace sootc