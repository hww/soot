// Compiler.cpp
#include "Compiler.hpp"
#include "common/carbon/files/FunctionDesc.hpp"
#include "common/carbon/lib/Ptr.hpp"
#include "common/util/Log.hpp"
#include "files/BinaryFileBuilder.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Compiler/TypeCompiler.hpp"
#include "type_system/Type.hpp"
#include <fmt/format.h>

namespace sootc {

Compiler::Compiler(TypeSystem& ts, std::string module_name) 
    : ts_(ts), 
      builder_(module_name),           // ← инициализация builder_
      type_compiler_(ts, builder_) {   // ← передаем builder_ в type_compiler_
    setup_forms();
}

void Compiler::setup_forms() {
    m_forms = {
        {"define", &Compiler::compile_define},
        {"lambda", &Compiler::compile_lambda},
        {"begin", &Compiler::compile_begin},
        {"if", &Compiler::compile_if},
        {"quote", &Compiler::compile_quote},
        {"set!", &Compiler::compile_set},
        {"+", &Compiler::compile_add},
        {"-", &Compiler::compile_sub},
        {"*", &Compiler::compile_mul},
        {"/", &Compiler::compile_div},
        {"deftype", &Compiler::compile_deftype},
    };
}


std::shared_ptr<Module>  Compiler::compile_module(const script::Object& forms, Env* env) {
    
    auto current = forms;
    while (current.is_pair()) {
        auto form = current.as_pair()->car;
        
        // Компилируем форму
        RelocatableBuffer buffer = compile(form, env);
    
        current = current.as_pair()->cdr;
    }
    return builder_.build_module();
}

RelocatableBuffer Compiler::compile(const script::Object& form, Env* env) {
    // Сброс состояния
    m_instructions.clear();
    m_next_reg = 0;
    
    if (!form.is_pair()) {
        // Атом: число, символ
        if (form.is_number()) {
            return compile_number(form, env);
        } else if (form.is_symbol()) {
            return compile_symbol(form, env);
        }
        lg::error("Unsupported atom: {}", form.print());
        return {};
    }
    
    auto pair = form.as_pair();
    auto& head = pair->car;
    
    if (!head.is_symbol()) {
        // Вызов функции
        return compile_call(form, env);
    }
    
    std::string op = head.as_symbol().c_str();
    auto it = m_forms.find(op);
    
    if (it != m_forms.end()) {
        // Вызов обработчика формы
        return it->second(this, form, pair->cdr, env);
    }
    
    // Обычный вызов функции
    return compile_call(form, env);
}

RelocatableBuffer Compiler::compile_define(const script::Object& form, const script::Object& rest, Env* env) {
    // (define name (lambda ...))
    if (!rest.is_pair()) {
        lg::error("define requires name and value");
        return {};
    }
    
    auto name_form = rest.as_pair()->car;
    auto value_form = rest.as_pair()->cdr.as_pair()->car;
    
    if (!name_form.is_symbol()) {
        lg::error("define name must be a symbol");
        return {};
    }
    
    std::string func_name = name_form.as_symbol().c_str();
    
    // Компилируем lambda
    auto result = compile(value_form, env);

    if (result.size() > 0) {
        // Добавляем дефиницию в билдер
        builder_.add_definition(func_name, "function", std::move(result), SymbolFlags::Export);
    }

    return result;
}

RelocatableBuffer Compiler::compile_lambda(const script::Object& form, const script::Object& rest, Env* env) {
    FunctionCompiler fn_compiler(ts_);
    return fn_compiler.compile(form, "lambda", env);
}

RelocatableBuffer Compiler::compile_begin(const script::Object& form, const script::Object& rest, Env* env) {
    // (begin expr1 expr2 ...)
    auto current = rest;
    RelocatableBuffer last_result;
    
    while (current.is_pair()) {
        auto expr = current.as_pair()->car;
        last_result = compile(expr, env);
        current = current.as_pair()->cdr;
    }
    
    return last_result;
}

RelocatableBuffer Compiler::compile_if(const script::Object& form, const script::Object& rest, Env* env) {
    // (if cond then else) - упрощенная реализация
    if (!rest.is_pair()) {
        lg::error("Invalid if form");
        return {};
    }
    
    auto cond_form = rest.as_pair()->car;
    auto then_form = rest.as_pair()->cdr.as_pair()->car;
    auto else_form = rest.as_pair()->cdr.as_pair()->cdr.as_pair()->car;
    
    // Простая заглушка - компилируем then
    (void)cond_form;
    (void)else_form;
    
    return compile(then_form, env);
}

RelocatableBuffer Compiler::compile_quote(const script::Object& form, const script::Object& rest, Env* env) {
    // (quote value) - возвращаем значение как константу
    if (!rest.is_pair()) {
        lg::error("Invalid quote form");
        return {};
    }
    
    auto value = rest.as_pair()->car;
    
    if (value.is_number()) {
        return compile_number(value, env);
    }
    
    lg::warn("Quote of non-number not fully implemented: {}", value.print());
    return {};
}

RelocatableBuffer Compiler::compile_set(const script::Object& form, const script::Object& rest, Env* env) {
    // (set! var value)
    if (!rest.is_pair()) {
        lg::error("Invalid set! form");
        return {};
    }
    
    auto var_form = rest.as_pair()->car;
    auto value_form = rest.as_pair()->cdr.as_pair()->car;
    
    // Просто компилируем значение
    return compile(value_form, env);
}

RelocatableBuffer Compiler::compile_add(const script::Object& form, const script::Object& rest, Env* env) {
    if (!rest.is_pair()) {
        lg::error("+ requires arguments");
        return {};
    }
    
    auto left_form = rest.as_pair()->car;
    auto right_form = rest.as_pair()->cdr.as_pair()->car;
    
    int left_reg = alloc_reg();
    int right_reg = alloc_reg();
    int dest_reg = alloc_reg();
    
    // Сохраняем инструкции для сложения
    m_instructions.clear();
    m_next_reg = 0;
    
    compile(left_form, env);
    compile(right_form, env);
    emit_binary_op(Opcode::ADD_INT, dest_reg, left_reg, right_reg);
    emit_return(dest_reg);
    
    return build_function_buffer(m_instructions);
}

RelocatableBuffer Compiler::compile_sub(const script::Object& form, const script::Object& rest, Env* env) {
    // Аналогично add
    return compile_add(form, rest, env);
}

RelocatableBuffer Compiler::compile_mul(const script::Object& form, const script::Object& rest, Env* env) {
    return compile_add(form, rest, env);
}

RelocatableBuffer Compiler::compile_div(const script::Object& form, const script::Object& rest, Env* env) {
    return compile_add(form, rest, env);
}

RelocatableBuffer Compiler::compile_number(const script::Object& form, Env* env) {
    m_instructions.clear();
    m_next_reg = 0;
    
    int reg = alloc_reg();
    int64_t val = form.as_integer();
    emit_load_imm(reg, val);
    emit_return(reg);
    
    return build_function_buffer(m_instructions);
}

RelocatableBuffer Compiler::compile_symbol(const script::Object& form, Env* env) {
    std::string name = form.as_symbol().c_str();
    int reg = lookup_reg(name, env);
    
    if (reg < 0) {
        lg::error("Undefined variable: {}", name);
        return {};
    }
    
    m_instructions.clear();
    m_next_reg = 0;
    
    emit_return(reg);
    
    return build_function_buffer(m_instructions);
}

RelocatableBuffer Compiler::compile_call(const script::Object& form, Env* env) {
    // Заглушка для вызовов функций
    lg::debug("Function call: {}", form.print());
    return compile_number(script::Object::make_integer(0), env);
}

RelocatableBuffer Compiler::compile_deftype(const Object& form, const Object& rest, Env* env) {
    (void)rest;
    return type_compiler_.compile(form, form.as_pair()->cdr, env);
}


int Compiler::alloc_reg() {
    return m_next_reg++;
}

int Compiler::lookup_reg(const std::string& name, Env* env) {
    return env->lookup_local(name);
}

void Compiler::emit(Instruction instr) {
    m_instructions.push_back(instr);
}

void Compiler::emit_load_imm(int reg, int64_t value) {
    emit(Instruction::create_imm(Opcode::LOAD_IMMEDIATE_INT, reg, value));
}

void Compiler::emit_binary_op(Opcode op, int dest, int left, int right) {
    emit(Instruction::create_abc(op, dest, left, right));
}

void Compiler::emit_return(int reg) {
    emit(Instruction::create_a(Opcode::RETURN, reg));
}

RelocatableBuffer Compiler::build_function_buffer(const std::vector<Instruction>& instructions) {
    RelocatableBuffer buffer;
    
    FunctionDesc desc;
    desc.code_count = static_cast<u32>(instructions.size());
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
    buffer.add_bytes(instructions.data(), instructions.size() * sizeof(Instruction));
    
    Ptr<Instruction>* code_ptr_field = 
        reinterpret_cast<Ptr<Instruction>*>(buffer.data() + code_ptr_offset);
    code_ptr_field->offset = code_start;
    
    return buffer;
}

RelocatableBuffer Compiler::build_result() {
    return build_function_buffer(m_instructions);
}

} // namespace sootc