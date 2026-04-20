#include "FunctionNode.hpp"
#include "ExpressionNode.hpp"  
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include "sootc/node/Node.hpp"
#include "sootc/node/ExpressionNode.hpp"
#include "sootc/libs/CompareOp.hpp"

namespace sootc {

// ========================================================================
// Constructor
// ========================================================================
FunctionNode::FunctionNode(const std::string& name) : Node(NodeType::FunctionNode), m_name(name) {}

// ========================================================================
// to_string
// ========================================================================
std::string FunctionNode::to_string() const {
    return "FunctionNode(name=" + m_name + 
           ", params=" + std::to_string(m_param_count) +
           ", locals=" + std::to_string(m_variables.size() - m_param_count) + ")";
}

// ========================================================================
// Параметры
// ========================================================================
void FunctionNode::add_parameter(const std::string& name, Type* type) {
    u8 reg = static_cast<u8>(m_variables.size());
    m_variables.emplace_back(name, type, reg, true);
    m_variable_index[name] = m_variables.size() - 1;
    m_param_count++;
    m_param_map[name] = reg;
}

// ========================================================================
// Локальные переменные
// ========================================================================
void FunctionNode::add_local_variable(const std::string& name, Type* type) {
    u8 reg = static_cast<u8>(m_variables.size());
    m_variables.emplace_back(name, type, reg, false);
    m_variable_index[name] = m_variables.size() - 1;
}

const VariableInfo* FunctionNode::lookup_variable(const std::string& name) const {
    auto it = m_variable_index.find(name);
    if (it != m_variable_index.end()) {
        return &m_variables[it->second];
    }
    return nullptr;
}

u8 FunctionNode::get_variable_reg(const std::string& name) const {
    auto* info = lookup_variable(name);
    return info ? info->reg() : 0;
}

Type* FunctionNode::get_variable_type(const std::string& name) const {
    auto* info = lookup_variable(name);
    return info ? info->type() : nullptr;
}

// ========================================================================
// Временные регистры
// ========================================================================
u8 FunctionNode::alloc_temp_reg(Type* type) {
    (void)type;
    u8 base = static_cast<u8>(m_variables.size());
    return static_cast<u8>(base + m_next_temp_reg++);
}

void FunctionNode::set_temp_reg(const Node* node, u8 reg) {
    m_temp_regs[node] = reg;
}

u8 FunctionNode::get_temp_reg(const Node* node) const {
    return m_temp_regs.at(node);
}

// ========================================================================
// Общий доступ к регистрам
// ========================================================================
u8 FunctionNode::get_reg(const Node* node) const {
    // Сначала ищем среди временных регистров
    auto tit = m_temp_regs.find(node);
    if (tit != m_temp_regs.end()) return tit->second;
    
    // Потом среди переменных
    if (auto* var_info = lookup_variable(node->to_string())) {
        return var_info->reg();
    }
    
    return 0;
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

std::string FunctionNode::create_unique_label(const std::string& prefix) { 
    return prefix + "_" + std::to_string(m_label_counter++); 
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

void FunctionNode::add_compare(u8 left, u8 right, CompareOp op) {
    switch (op) {
        case CompareOp::EQ:
            add_instruction(Opcode::IEqual, left, left, right);
            break;
        case CompareOp::NE:
            add_instruction(Opcode::INotEqual, left, left, right);
            break;
        case CompareOp::LT:
            add_instruction(Opcode::ILessThan, left, left, right);
            break;
        case CompareOp::LE:
            add_instruction(Opcode::ILessThanEqual, left, left, right);
            break;
        case CompareOp::GT:
            add_instruction(Opcode::IGreaterThan, left, left, right);
            break;
        case CompareOp::GE:
            add_instruction(Opcode::IGreaterThanEqual, left, left, right);
            break;
    }
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
    m_temp_regs.clear();
    m_next_temp_reg = 0;
    
    body->emit(*this);
    resolve_branches();
}

void FunctionNode::emit_body() {
    if (!m_body) return;
    emit_body(m_body.get());
}

// ========================================================================
// Сериализация
// ========================================================================
ProgramBinaryElement FunctionNode::build_binary(const std::string& module_name, GlobalState& state) {
    (void)module_name;
    
    // Вычисляем размер
    u64 total_size = sizeof(sid64) + sizeof(ScriptLambda) + 
                     m_instructions.size() * sizeof(Instruction) + 
                     m_constants.size() * sizeof(u64);
    
    carbon::ProgramBinaryElement element(total_size);
    
    // Устанавливаем entry point
    element.m_entry = {
        .m_nameID = SID(m_name.c_str()),
        .m_typeId = SCRIPT_LAMBDA_SID,
        .m_entryPtr = nullptr
    };
    
    // 1. Записываем sid64 (тип ScriptLambda)
    sid64 script_lambda_sid = SCRIPT_LAMBDA_SID;
    element.push_bytes(script_lambda_sid, 0b0);
    
    // 2. Создаем и записываем ScriptLambda структуру
    ScriptLambda lambda{};
    lambda.m_pInstruction = nullptr;  // Будет заполнено при линковке
    lambda.m_pSymbols = nullptr;      // Будет заполнено при линковке
    lambda.m_typeId = FUNCTION_SID;
    lambda.m_sum = 12 + 4 * (m_instructions.size() + m_constants.size());
    lambda.m_funcName = SID(m_name.c_str());
    lambda.m_instructionFlag = DEADBEEF;
    lambda.m_always0_2 = 0;
    lambda.m_numInstructions = static_cast<u32>(m_instructions.size());
    lambda.m_neg1 = -1;
    lambda.m_sidGlobal = GLOBAL_SID;
    lambda.m_always0_3 = 0;
    
    // Записываем структуру с флагами релокации для указателей
    element.push_bytes(lambda, 0b0, 0b0, 0b0, 0b0, 0b0, 0b0, 0b0, 0b0, 0b0, 0b0);
    
    // 3. Записываем инструкции
    for (const Instruction& instr : m_instructions) {
        element.push_bytes(instr, 0b0);
    }
    
    // 4. Записываем таблицу констант
    for (size_t i = 0; i < m_constants.size(); ++i) {
        // Определяем тип константы по m_constants_kind
        u8 kind = m_constants_kind[i];
        if (kind == static_cast<u8>(ConstKind::STRING)) {
            element.insert_string_offset();
            element.push_bytes(m_constants[i], 0b1);
        } else {
            element.push_bytes(m_constants[i], 0b0);
        }
    }
    
    return element;
}


} // namespace sootc