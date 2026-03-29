// FunctionCompiler.cpp
#include "FunctionCompiler.hpp"
#include "common/carbon/files/FunctionDesc.hpp"
#include "common/carbon/lib/Ptr.hpp"
#include "common/util/Log.hpp"

namespace sootc {

FunctionCompiler::FunctionCompiler(TypeSystem& ts) : ts_(ts) {}

RelocatableBuffer FunctionCompiler::compile(const script::Object& form, 
                                             const std::string& func_name, 
                                             Env* env) {
    if (!form.is_pair()) {
        lg::error("Expected lambda form");
        return {};
    }
    
    instructions_.clear();
    
    auto lambda = form.as_pair();
    auto args_form = lambda->cdr.as_pair()->car;
    auto body_form = lambda->cdr.as_pair()->cdr;
    
    // Подсчет количества аргументов
    int arg_count = 0;
    if (args_form.is_pair()) {
        auto current = args_form;
        while (current.is_pair()) {
            arg_count++;
            current = current.as_pair()->cdr;
        }
    }
    
    // Создание функционального окружения с 3 аргументами
    FunctionEnv func_env(env, func_name, arg_count);
    
    // Регистрация аргументов с индексами
    if (args_form.is_pair()) {
        int arg_index = 0;
        auto current = args_form;
        while (current.is_pair()) {
            auto arg = current.as_pair()->car;
            if (arg.is_symbol()) {
                func_env.add_arg(arg.as_symbol().c_str(), arg_index);
                arg_index++;
            }
            current = current.as_pair()->cdr;
        }
    }
    
    // Следующий свободный регистр для локальных переменных
    next_reg_ = func_env.next_local_reg();
    
    // Компиляция тела
    if (body_form.is_pair()) {
        auto body = body_form.as_pair()->car;
        int result_reg = compile_form(body, &func_env);
        if (result_reg >= 0) {
            emit_return(result_reg);
        }
    }
    
    if (instructions_.empty() || instructions_.back().opcode != Opcode::RETURN) {
        emit_return(0);
    }
    
    return build_buffer();
}

int FunctionCompiler::compile_form(const script::Object& form, Env* env) {
    if (form.is_number()) {
        return compile_number(form, env);
    } else if (form.is_symbol()) {
        return compile_symbol(form, env);
    } else if (form.is_pair()) {
        auto pair = form.as_pair();
        auto first = pair->car;
        
        if (first.is_symbol()) {
            std::string op = first.as_symbol().c_str();
            if (op == "+" || op == "-" || op == "*" || op == "/") {
                return compile_binary_op(form, env, op);
            }
        }
    }
    
    lg::error("Unsupported form: {}", form.print());
    return -1;
}

int FunctionCompiler::compile_number(const script::Object& form, Env* env) {
    (void)env;
    int reg = alloc_reg();
    int64_t val = form.as_integer();
    emit_load_imm(reg, val);
    return reg;
}

int FunctionCompiler::compile_symbol(const script::Object& form, Env* env) {
    std::string name = form.as_symbol().c_str();
    int reg = lookup_reg(name, env);
    
    if (reg < 0) {
        lg::error("Undefined variable: {}", name);
        return -1;
    }
    
    return reg;
}

int FunctionCompiler::compile_binary_op(const script::Object& form, Env* env, const std::string& op) {
    auto pair = form.as_pair();
    auto args = pair->cdr;
    
    if (!args.is_pair()) {
        lg::error("Binary op requires arguments");
        return -1;
    }
    
    auto left_form = args.as_pair()->car;
    auto right_form = args.as_pair()->cdr.as_pair()->car;
    
    int left_reg = compile_form(left_form, env);
    int right_reg = compile_form(right_form, env);
    int dest_reg = alloc_reg();
    
    Opcode opcode;
    if (op == "+") opcode = Opcode::ADD_INT;
    else if (op == "-") opcode = Opcode::SUB_INT;
    else if (op == "*") opcode = Opcode::MUL_INT;
    else opcode = Opcode::DIV_INT;
    
    emit_binary_op(opcode, dest_reg, left_reg, right_reg);
    
    return dest_reg;
}

int FunctionCompiler::alloc_reg() {
    return next_reg_++;
}

int FunctionCompiler::lookup_reg(const std::string& name, Env* env) {
    return env->lookup_local(name);
}

void FunctionCompiler::emit(Instruction instr) {
    instructions_.push_back(instr);
}

void FunctionCompiler::emit_load_imm(int reg, int64_t value) {
    emit(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, reg, value));
}

void FunctionCompiler::emit_binary_op(Opcode op, int dest, int left, int right) {
    emit(Instruction::create_abc(op, dest, left, right));
}

void FunctionCompiler::emit_return(int reg) {
    emit(Instruction::create_a(Opcode::RETURN, reg));
}

RelocatableBuffer FunctionCompiler::build_buffer() {
    RelocatableBuffer buffer;
    
    FunctionDesc desc;
    desc.code_count = static_cast<u32>(instructions_.size());
    desc.data_size = 0;
    desc.debug_count = 0;
    desc.reserved = 0;
    desc.owner_module = nullptr;
    desc.code_ptr = nullptr;
    desc.data_ptr = nullptr;
    desc.debug_ptr = nullptr;
    
    u32 header_start = buffer.size();
    buffer.add_bytes(&desc, sizeof(FunctionDesc));
    
    u32 code_ptr_offset = header_start + offsetof(FunctionDesc, code_ptr);
    buffer.add_relocatable_offset(code_ptr_offset);
    
    u32 code_start = buffer.size();
    buffer.add_bytes(instructions_.data(), instructions_.size() * sizeof(Instruction));
    
    Ptr<Instruction>* code_ptr_field = 
        reinterpret_cast<Ptr<Instruction>*>(buffer.data() + code_ptr_offset);
    code_ptr_field->offset = code_start;
    
    return buffer;
}

} // namespace sootc