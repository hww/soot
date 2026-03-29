// Compiler.hpp
#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "Env.hpp"
#include "files/BinaryFileBuilder.hpp"
#include "sootc/Compiler/TypeCompiler.hpp"
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>

using namespace carbon::files;
using namespace carbon::vm;
using namespace carbon::modules;

namespace sootc {

class Compiler {
public:
    Compiler(TypeSystem& ts, std::string module_name);
    
    std::shared_ptr<Module> compile_module(const script::Object& form, Env* env);

    // Главный метод компиляции
    RelocatableBuffer compile(const script::Object& form, Env* env);
    
    // Обработчики форм (как в эталоне)
    RelocatableBuffer compile_define(const script::Object& form, const script::Object& rest, Env* env);
    RelocatableBuffer compile_lambda(const script::Object& form, const script::Object& rest, Env* env);
    RelocatableBuffer compile_begin(const script::Object& form, const script::Object& rest, Env* env);
    RelocatableBuffer compile_if(const script::Object& form, const script::Object& rest, Env* env);
    RelocatableBuffer compile_quote(const script::Object& form, const script::Object& rest, Env* env);
    RelocatableBuffer compile_set(const script::Object& form, const script::Object& rest, Env* env);
    
    // Бинарные операции
    RelocatableBuffer compile_add(const script::Object& form, const script::Object& rest, Env* env);
    RelocatableBuffer compile_sub(const script::Object& form, const script::Object& rest, Env* env);
    RelocatableBuffer compile_mul(const script::Object& form, const script::Object& rest, Env* env);
    RelocatableBuffer compile_div(const script::Object& form, const script::Object& rest, Env* env);
    
    // Атомы
    RelocatableBuffer compile_number(const script::Object& form, Env* env);
    RelocatableBuffer compile_symbol(const script::Object& form, Env* env);
    RelocatableBuffer compile_call(const script::Object& form, Env* env);
    
    // Тип
    RelocatableBuffer compile_deftype(const Object& form, const Object& rest, Env* env);

    // Сборка результата
    RelocatableBuffer build_result();
    
    void add_definition(const std::string& name, const std::string& type, RelocatableBuffer&& buffer) {
        builder_.add_definition(name, type, std::move(buffer), SymbolFlags::Export);
    }

private:
    TypeSystem& ts_;
    BinaryFileBuilder builder_;
    TypeCompiler type_compiler_;

    // Таблица диспетчеризации (как g_goal_forms)
    using FormHandler = std::function<RelocatableBuffer(Compiler*, const script::Object&, const script::Object&, Env*)>;
    std::unordered_map<std::string, FormHandler> m_forms;
    void setup_forms();
    
    // Текущее состояние компиляции
    std::vector<Instruction> m_instructions;
    int m_next_reg = 0;
    
    // Вспомогательные методы
    int alloc_reg();
    int lookup_reg(const std::string& name, Env* env);
    void emit(Instruction instr);
    void emit_load_imm(int reg, int64_t value);
    void emit_binary_op(Opcode op, int dest, int left, int right);
    void emit_return(int reg);
    
    // Построение FunctionDesc буфера
    RelocatableBuffer build_function_buffer(const std::vector<Instruction>& instructions);
};

} // namespace sootc