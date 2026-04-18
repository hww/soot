#pragma once

#include "common/sootc/IR/IR_Value.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include "vm/Instructions.hpp"
#include <vector>

namespace sootc {

class FunctionEnv;
class Compiler;

// Константы для бинарника
constexpr u64 DEADBEEF = 0xDEAD'BEEF'1337'F00D;
constexpr sid64 GLOBAL_SID = SID("global");
constexpr sid64 FUNCTION_SID = SID("function");
constexpr sid64 SCRIPT_LAMBDA_SID = SID("script-lambda");

/*!
 * IR_FunctionValue - значение функции, которое умеет:
 * 1. Хранить тело функции (список выражений)
 * 2. Генерировать инструкции (emit)
 * 3. Сериализоваться в бинарник (serialize)
 */
class IR_FunctionValue : public IR_Value {
public:
    enum class SymbolTablePointerKind {
        NONE,
        STRING,
        GENERAL,
    };

    explicit IR_FunctionValue(FunctionEnv* env);
    ~IR_FunctionValue() = default;

    // ========================================================================
    // IR_Value interface
    // ========================================================================
    std::string to_string() const override;
    void resolve(Compiler* c) override;
    ProgramBinaryElement serialize(Compiler* c) override;

    // ========================================================================
    // Body management
    // ========================================================================
    void add_expression(IR_Value* expr);
    void set_body(const std::vector<IR_Value*>& body) { m_body = body; }
    const std::vector<IR_Value*>& body() const { return m_body; }

    // ========================================================================
    // Code generation (emit phase)
    // ========================================================================
    void emit(Env& env, Compiler* compiler) override;
    
    // ========================================================================
    // Getters
    // ========================================================================
    FunctionEnv* get_env() const { return m_env; }
    const std::string& get_name() const { return m_name; }

private:
    // ========================================================================
    // Генерация инструкций (вспомогательные методы)
    // ========================================================================
    void emit_expression(IR_Value* expr, Compiler* compiler);
    void emit_instruction(Opcode opcode, u8 destination, u8 operand1 = 0, u8 operand2 = 0);
    u16 add_to_symbol_table(u64 value, SymbolTablePointerKind kind = SymbolTablePointerKind::NONE);
    u64 get_scriptlambda_sum() const noexcept;
    void clear();
    
    // ========================================================================
    // Сериализация
    // ========================================================================
    ProgramBinaryElement build_binary(const std::string& name);

    // ========================================================================
    // Данные
    // ========================================================================
    std::string m_name;
    FunctionEnv* m_env;
    std::vector<IR_Value*> m_body;
    
    // Результаты генерации кода
    std::vector<Instruction> m_instructions;
    std::vector<u64> m_symbolTable;
    std::vector<SymbolTablePointerKind> m_symbolTableEntryPointers;
    
    static int s_lambda_index;
};

} // namespace sootc