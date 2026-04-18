#include "sootc/IR/IR_FunctionValue.hpp"
#include "common/sootc/Env/FunctionEnv.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include "fmt/format.h"
#include <numeric>

namespace sootc {

int IR_FunctionValue::s_lambda_index = 1;

// ============================================================================
// Construction
// ============================================================================

IR_FunctionValue::IR_FunctionValue(FunctionEnv* env)
    : IR_Value(env->get_return_type() ? env->get_return_type() : TypeSystem::instance().lookup_type("void"))
    , m_name(env->get_name())
    , m_env(env) {}

// ============================================================================
// IR_Value interface
// ============================================================================

std::string IR_FunctionValue::to_string() const {
    return "function:" + m_name;
}

void IR_FunctionValue::resolve(Compiler* c) {
    for (auto* expr : m_body) {
        if (expr) expr->resolve(c);
    }
}

ProgramBinaryElement IR_FunctionValue::serialize(Compiler* c) {
    if (m_instructions.empty()) {
        emit(*m_env, c);
    }
    return build_binary(m_name);
}

// ============================================================================
// Body management
// ============================================================================

void IR_FunctionValue::add_expression(IR_Value* expr) {
    m_body.push_back(expr);
}

// ============================================================================
// Code generation (emit phase)
// ============================================================================

void IR_FunctionValue::emit(Env& env, Compiler* compiler) {
    auto* fn_env = dynamic_cast<FunctionEnv*>(&env);
    if (!fn_env) return;
    
    clear();
    
    for (auto* expr : m_body) {
        emit_expression(expr, compiler);
    }
}

void IR_FunctionValue::emit_expression(IR_Value* expr, Compiler* compiler) {
    if (!expr) return;
    
    // Вызываем emit у выражения
    // Для этого нужно получить FunctionEnv из m_env
    expr->emit(*m_env, compiler);
}

void IR_FunctionValue::emit_instruction(Opcode opcode, u8 destination, u8 operand1, u8 operand2) {
    m_instructions.emplace_back(InstructionFactory::abc(opcode, destination, operand1, operand2));
}

u16 IR_FunctionValue::add_to_symbol_table(u64 value, SymbolTablePointerKind kind) {
    auto existing = std::find(m_symbolTable.begin(), m_symbolTable.end(), value);
    if (existing != m_symbolTable.end()) {
        return static_cast<u16>(std::distance(m_symbolTable.begin(), existing));
    } else {
        u16 current_size = static_cast<u16>(m_symbolTable.size());
        m_symbolTable.push_back(value);
        m_symbolTableEntryPointers.push_back(kind);
        return current_size;
    }
}

u64 IR_FunctionValue::get_scriptlambda_sum() const noexcept {
    return sizeof(ScriptLambda) + m_instructions.size() + m_symbolTable.size();
}

void IR_FunctionValue::clear() {
    m_instructions.clear();
    m_symbolTable.clear();
    m_symbolTableEntryPointers.clear();
}

// ============================================================================
// Serialization
// ============================================================================

ProgramBinaryElement IR_FunctionValue::build_binary(const std::string& name) {
    u64 total_size = sizeof(sid64) + sizeof(ScriptLambda) + 
                     m_instructions.size() * sizeof(Instruction) + 
                     m_symbolTable.size() * sizeof(u64);
    
    ProgramBinaryElement element(total_size);
    element.m_entry = {
        .m_nameID = SID(name.c_str()),
        .m_typeId = SCRIPT_LAMBDA_SID,
        .m_entryPtr = nullptr
    };
    
    sid64 script_lambda_sid = SCRIPT_LAMBDA_SID;
    element.push_bytes(script_lambda_sid, 0b0);
    
    ScriptLambda lambda{};
    lambda.m_pInstruction = nullptr;
    lambda.m_pSymbols = nullptr;
    lambda.m_typeId = FUNCTION_SID;
    lambda.m_sum = get_scriptlambda_sum();
    lambda.m_funcName = SID(name.c_str());
    lambda.m_instructionFlag = DEADBEEF;
    lambda.m_always0_2 = 0;
    lambda.m_numInstructions = static_cast<u32>(m_instructions.size());
    lambda.m_neg1 = -1;
    lambda.m_sidGlobal = GLOBAL_SID;
    lambda.m_always0_3 = 0;
    
    element.push_bytes(lambda, 0b0, 0b0, 0b0, 0b0, 0b0, 0b0, 0b0, 0b0, 0b0, 0b0);
    
    for (const Instruction& instr : m_instructions) {
        element.push_bytes(instr, 0b0);
    }
    
    for (u32 i = 0; i < m_symbolTable.size(); ++i) {
        if (m_symbolTableEntryPointers[i] == SymbolTablePointerKind::STRING) {
            element.insert_string_offset();
            element.push_bytes(m_symbolTable[i], 0b1);
        } else {
            element.push_bytes(m_symbolTable[i], 0b0);
        }
    }
    
    return element;
}

} // namespace sootc