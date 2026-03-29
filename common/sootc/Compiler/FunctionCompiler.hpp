// FunctionCompiler.hpp
#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "Env.hpp"
#include <vector>
#include <string>
#include <unordered_map>

using namespace carbon::files;
using namespace carbon::vm;

namespace sootc {

class FunctionCompiler {
public:
    FunctionCompiler(TypeSystem& ts);
    
    // Компиляция lambda формы в RelocatableBuffer
    RelocatableBuffer compile(const script::Object& form, const std::string& func_name, Env* env);
    
private:
    TypeSystem& ts_;
    
    // Состояние компиляции текущей функции
    std::vector<Instruction> instructions_;
    int next_reg_ = 0;
    
    // Вспомогательные методы
    int alloc_reg();
    int lookup_reg(const std::string& name, Env* env);
    void emit(Instruction instr);
    void emit_load_imm(int reg, int64_t value);
    void emit_binary_op(Opcode op, int dest, int left, int right);
    void emit_return(int reg);
    
    // Компиляция формы в регистр
    int compile_form(const script::Object& form, Env* env);
    int compile_number(const script::Object& form, Env* env);
    int compile_symbol(const script::Object& form, Env* env);
    int compile_binary_op(const script::Object& form, Env* env, const std::string& op);
    
    // Сборка результата
    RelocatableBuffer build_buffer();
};

} // namespace sootc