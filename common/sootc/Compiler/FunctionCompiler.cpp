#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "common/carbon/files/FunctionDesc.hpp" 
#include "common/carbon/files/RelocatableBuffer.hpp" 
#include "sootc/IR/IR_Value.hpp"
#include "sootc/IR/IR_Node.hpp"

using namespace ::carbon::files;
using namespace ::carbon::vm;

namespace sootc {

FunctionCompiler::FunctionCompiler(TypeSystem& ts, Compiler* compiler)
    : ts_(ts), compiler_(compiler) {}

// Сигнатура восстановлена: form — это (defun ...), rest — всё что после
IR_Value* FunctionCompiler::declare(const script::Object& form, 
                                   const script::Object& rest, 
                                   Env* env) {
    (void)form;
    
    // Распаковываем rest: ( (аргументы) тело... )
    auto args_list = rest.as_pair()->car;
    auto body_forms = rest.as_pair()->cdr;

    // Имя берем из контекста или генерируем (для анонимных)
    auto* f_env = new FunctionEnv(env, "lambda");
    
    // ВАЖНО: Сохраняем хвост списка (тело) для второго прохода
    f_env->set_source_form(body_forms); 

    u32 arg_idx = 24; 
    auto current_arg = args_list;

    while (current_arg.is_pair()) {
        auto arg = current_arg.as_pair()->car;
        std::string arg_name;
        Type* type = ts_.lookup_type("object"); 

        if (arg.is_symbol()) {
            arg_name = arg.as_symbol().c_str();
        } else if (arg.is_pair()) {
            auto decl = arg.as_pair();
            arg_name = decl->car.as_symbol().c_str();
            if (decl->cdr.is_pair()) {
                type = ts_.lookup_type(decl->cdr.as_pair()->car.as_symbol().c_str());
            }
        }

        f_env->define_argument(arg_name, type, arg_idx++);
        current_arg = current_arg.as_pair()->cdr;
    }

    return new IR_FunctionValue(f_env);
}

void FunctionCompiler::compile_body(IR_FunctionValue* f_val) {
    auto* f_env = f_val->get_env();
    auto body_forms = f_env->source_form(); // Достаем то, что сохранили в declare
    
    IR_Value* last_val = nullptr;
    auto current_f = body_forms;

    while (current_f.is_pair()) {
        last_val = compiler_->declare(current_f.as_pair()->car, f_env);
        current_f = current_f.as_pair()->cdr;
    }

    if (last_val) {
        f_env->emit(new IR_Return(last_val));
    } else {
        Type* obj_type = ts_.lookup_type("object");
        f_env->emit(new IR_Return(new IR_Const(obj_type, static_cast<s64>(0))));
    }
}

// Метод build остается без изменений, он работает с FunctionEnv
RelocatableBuffer FunctionCompiler::build(FunctionEnv* fe) {
    FunctionDescBuilder func_builder; 
    std::unordered_map<IR_Value*, u32> reg_map;
    
    const u32 ARG_START = 24; 
    const u32 LOCAL_START = 0;

    u32 next_arg = ARG_START;
    for (auto* arg_val : fe->params()) { 
        reg_map[arg_val] = next_arg++;
    }

    u32 next_local = LOCAL_START;
    for (auto* node : fe->nodes()) {
        for (auto* val : node->get_used_values()) {
            if (val && reg_map.find(val) == reg_map.end()) {
                if (next_local >= ARG_START) {
                    lg::error("Function {}: Out of local registers!", fe->name());
                }
                reg_map[val] = next_local++;
            }
        }
    }

    for (auto* node : fe->nodes()) {
        node->generate(func_builder, reg_map);
    }
    
    auto instructions = func_builder.get_instructions();
    RelocatableBuffer buffer;

    FunctionDesc desc{}; 
    desc.code_count = static_cast<u32>(instructions.size());
    desc.code_ptr = reinterpret_cast<Instruction*>(sizeof(FunctionDesc));

    buffer.add_bytes(&desc, sizeof(desc));
    buffer.add_relocatable_offset(offsetof(FunctionDesc, code_ptr));

    if (!instructions.empty()) {
        buffer.add_bytes(instructions.data(), instructions.size() * sizeof(Instruction));
    }
    
    return buffer;
}

} // namespace sootc