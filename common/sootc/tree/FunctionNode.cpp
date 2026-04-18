#include "FunctionNode.hpp"
#include "ExpressionNode.hpp"  
#include "common/carbon/file/ProgramBinaryElement.hpp"

namespace sootc {

// ========================================================================
// Constructor
// ========================================================================
FunctionNode::FunctionNode(const std::string& name) : m_name(name) {}

// ========================================================================
// to_string
// ========================================================================
std::string FunctionNode::to_string() const {
    return "FunctionNode(name=" + m_name + ")";
}

// ========================================================================
// Параметры
// ========================================================================
void FunctionNode::add_parameter(const std::string& name, Type* type) {
    u8 reg = static_cast<u8>(m_params.size());
    m_params.push_back(new Parameter(name, type, reg));
    m_param_map[name] = reg;
}

// ========================================================================
// Регистры
// ========================================================================
u8 FunctionNode::alloc_reg(Type* type) {
    u8 idx = static_cast<u8>(m_next_reg++);
    m_locals.push_back(std::make_unique<LocalVariable>(type, idx));
    return idx;
}

u8 FunctionNode::get_reg(const Node* node) const {
    return m_reg_map.at(node);
}

void FunctionNode::set_reg(const Node* node, u8 reg) {
    m_reg_map[node] = reg;
}

// ========================================================================
// Константы
// ========================================================================
u16 FunctionNode::add_constant(u64 value, ConstKind kind) {
    auto it = std::find(m_constants.begin(), m_constants.end(), value);
    if (it != m_constants.end()) {
        return static_cast<u16>(it - m_constants.begin());
    }
    u16 idx = static_cast<u16>(m_constants.size());
    m_constants.push_back(value);
    m_constants_kind.push_back(static_cast<u8>(kind));
    return idx;
}

// ========================================================================
// Инструкции
// ========================================================================
void FunctionNode::add_instruction(Opcode op, u8 dest, u8 src1, u8 src2) {
    m_instructions.emplace_back(InstructionFactory::abc(op, dest, src1, src2));
}

void FunctionNode::add_instruction_imm_u16(Opcode op, u8 dest, u16 imm) {
    Instruction instr;
    instr.opcode = op;
    instr.a = dest;
    instr.imm16 = imm;
    m_instructions.push_back(instr);
}

// ========================================================================
// Метки
// ========================================================================
void FunctionNode::add_label(const std::string& name) {
    m_labels[name] = static_cast<u32>(m_instructions.size());
}

void FunctionNode::add_branch_reference(const std::string& label) {
    m_unresolved_branches.emplace_back(label, static_cast<u32>(m_instructions.size()));
    add_instruction(Opcode::Branch, 0, 0, 0);
}

void FunctionNode::resolve_branches() {
    for (auto& [label, pos] : m_unresolved_branches) {
        auto it = m_labels.find(label);
        if (it != m_labels.end()) {
            i32 offset = static_cast<i32>(it->second) - static_cast<i32>(pos);
            m_instructions[pos].imm16 = static_cast<i16>(offset);
        }
    }
    m_unresolved_branches.clear();
}

// ========================================================================
// Генерация кода
// ========================================================================
void FunctionNode::set_body(std::unique_ptr<ExpressionNode> body) {
    m_body = std::move(body);
}

void FunctionNode::emit_body(ExpressionNode* body) {
    if (!body) return;
    
    m_instructions.clear();
    m_constants.clear();
    m_reg_map.clear();
    m_next_reg = static_cast<int>(m_params.size());
    
    body->emit(*this);
    resolve_branches();
}

void FunctionNode::emit_body() {
    if (!m_body) return;
    emit_body(m_body.get());
}

// ========================================================================
// Сериализация (заглушка)
// ========================================================================
carbon::ProgramBinaryElement FunctionNode::build_binary(const std::string& module_name) {
    // 1. Вычисляем размер
    u64 total_size = sizeof(sid64) + sizeof(ScriptLambda) + 
                     m_instructions.size() * sizeof(Instruction) + 
                     m_constants.size() * sizeof(u64);
    
    // 2. Создаем элемент
    carbon::ProgramBinaryElement element(total_size);
    
    // 3. Заполняем заголовок, инструкции, константы
    // ... (как в старом FunctionCompiler::build)
    
    return element;
}

} // namespace sootc