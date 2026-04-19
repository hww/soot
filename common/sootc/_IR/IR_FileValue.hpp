// IR_FileValue.hpp
#pragma once

#include "common/sootc/IR/IR_Value.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include "common/carbon/file/BinaryFile.hpp"
#include "sootc/IR/IR_FunctionValue.hpp"
#include <vector>

namespace sootc {

class FileEnv;
class Compiler;

/*!
 * IR_FileValue - значение файла, которое умеет:
 * 1. Хранить все функции и статические данные файла
 * 2. Генерировать инструкции для top-level кода (emit)
 * 3. Сериализоваться в BinaryFile (serialize)
 */
class IR_FileValue : public IR_Value {
public:
    explicit IR_FileValue(FileEnv* env);
    ~IR_FileValue() = default;

    // ========================================================================
    // IR_Value interface
    // ========================================================================
    std::string to_string() const override;
    bool is_buildable() const override { return true; }
    
    void resolve(Compiler* c) override;
    void emit(Env& env, Compiler* compiler) override;
    ProgramBinaryElement serialize(Compiler* c) override;

    // ========================================================================
    // File management
    // ========================================================================
    void add_function(IR_FunctionValue* fn);
    void add_static(StaticObject* obj);
    
    const std::vector<IR_FunctionValue*>& functions() const { return m_functions; }
    const std::vector<StaticObject*>& statics() const { return m_statics; }
    
    FileEnv* get_env() const { return m_env; }
    const std::string& get_name() const { return m_name; }

private:
    // ========================================================================
    // Генерация top-level кода
    // ========================================================================
    void emit_top_level(Env& env, Compiler* compiler);
    void emit_function(IR_FunctionValue* fn, Env& env, Compiler* compiler);
    
    // ========================================================================
    // Сериализация
    // ========================================================================
    std::unique_ptr<BinaryFile> build_binary(Compiler* compiler);
    
    // ========================================================================
    // Данные
    // ========================================================================
    std::string m_name;
    FileEnv* m_env;
    std::vector<IR_FunctionValue*> m_functions;
    std::vector<StaticObject*> m_statics;
    
    // Результаты генерации кода (если нужны)
    std::vector<Instruction> m_top_level_instructions;
};

} // namespace sootc