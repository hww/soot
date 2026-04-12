#include "sootc/Compiler/FunctionCompiler.hpp"
#include "CommonTypes.hpp"
#include "fmt/format.h"
#include "lib/Variant.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "common/carbon/files/FunctionDesc.hpp" 
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "sootc/IR/IR_Value.hpp"
#include "sootc/IR/IR_Node.hpp"
#include "sootc/Env/MethodEnv.hpp"
#include "sootc/Env/StateEnv.hpp"
#include "sootc/Env/TypeEnv.hpp"
#include "sootc/IR/StaticObject.hpp"
#include "sootc/IR/StaticSegment.hpp"
#include "sootc/IR/IR_Node.hpp"
#include "type_system/Type.hpp"

using namespace ::carbon::files;
using namespace ::carbon::vm;

namespace sootc {

int FunctionCompiler::s_lambda_index = 1;


FunctionCompiler::FunctionCompiler(TypeSystem& ts, Compiler* compiler)
    : ts_(ts), compiler_(compiler) {}

// ============================================================================
// compile_function — один проход
// ============================================================================

IR_Value* FunctionCompiler::compile_function(const script::Object& form, 
                                               const script::Object& rest, 
                                               Env* env) {
    (void)form;
    
    auto args_list = rest.as_pair()->car;
    auto body_forms = rest.as_pair()->cdr;



    auto* f_env = new FunctionEnv(env, "lambda");
    
    // Парсим аргументы
    parse_arguments(args_list, f_env);
    
    // Компилируем тело (создаем IR_Node)
    compile_body(body_forms, f_env);
    
    return new IR_FunctionValue(f_env);
}

// ============================================================================
// compile_body — создает IR_Node для всех форм тела
// ============================================================================

void FunctionCompiler::compile_body(const script::Object& body_forms, FunctionEnv* env) {
    auto current = body_forms;
    IR_Value* last_val = nullptr;
    
    while (current.is_pair()) {
        last_val = compiler_->compile(current.as_pair()->car, env);
        current = current.as_pair()->cdr;
    }
    
    if (last_val) {
        env->emit(script::Object(), std::make_unique<IR_Return>(last_val));
    } else {
        Type* obj_type = ts_.lookup_type("object");
        env->emit(script::Object(), std::make_unique<IR_Return>(new IR_Const(obj_type, static_cast<s64>(0))));
    }
}

// ============================================================================
// parse_arguments
// ============================================================================

void FunctionCompiler::parse_arguments(const script::Object& args_form, FunctionEnv* env) {
    auto current = args_form;
    int arg_idx = env->params().size(); // учитываем уже существующие аргументы (например, this)
    
    while (current.is_pair()) {
        auto arg_decl = current.as_pair()->car;
        if (arg_decl.is_pair()) {
            std::string arg_name = arg_decl.as_pair()->car.as_symbol().c_str();
            std::string type_name = arg_decl.as_pair()->cdr.as_pair()->car.as_symbol().c_str();
            Type* arg_type = ts_.lookup_type(type_name);
            env->define_argument(arg_name, arg_type, arg_idx++);
        } else if (arg_decl.is_symbol()) {
            std::string arg_name = arg_decl.as_symbol().c_str();
            Type* arg_type = ts_.lookup_type("object");
            env->define_argument(arg_name, arg_type, arg_idx++);
        }
        current = current.as_pair()->cdr;
    }
}

// ============================================================================
// BUILD Phase (без изменений, но методы переименованы)
// ============================================================================

/**
 * Universal compiler for functions or methods
 */
RelocatableBuffer FunctionCompiler::build(FunctionEnv* fe, const std::string& name) {

    std::unordered_map<IR_Value*, u32> reg_map;
    
    // 1. Регистрируем аргументы
    u32 next_arg = ARG_REGISTERS_OFFSET;
    for (auto* arg_val : fe->params()) { 
        reg_map[arg_val] = next_arg++;
    }

    // 2. Регистрируем локальные переменные
    u32 next_local = LOCAL_REGISTERS_OFFSET;
    for (auto& node : fe->code()) {
        for (auto* val : node->get_used_values()) {
            if (val && reg_map.find(val) == reg_map.end()) {
                if (next_local >= ARG_REGISTERS_OFFSET) {
                    lg::error("Function {}: Out of local registers!", fe->get_name());
                }
                reg_map[val] = next_local++;
            }
        }
    }
    


    // 3. Создаеми буферы
    RelocatableBuffer result(name + "#descriptor", "function", true);
    RelocatableBuffer code_buffer(name + "#code","function", true);
    RelocatableBuffer data_buffer(name + "#data","void", true);
    RelocatableBuffer debug_buffer(name + "#debug","void", true);

    // 4. Генерируем байткод прямо в буфер
    StaticSegment statics;
    EmitContext ctx { code_buffer, statics, reg_map };
    for (auto& node : fe->code()) {
        lg::info("FunctionCompiler::build {}", node->to_string());
        node->generate(ctx);  // ← generate теперь принимает RelocatableBuffer
    }

    // 5. Заголовок функции
    FunctionDesc desc{}; 
    desc.name = StringId(fe->get_name());
    desc.code_count = static_cast<u32>(code_buffer.size() / sizeof(Instruction));
    desc.code_ptr.offset = sizeof(FunctionDesc);
    desc.data_size = 0;
    desc.data_ptr = nullptr;
    desc.debug_count = 0;
    desc.debug_ptr = nullptr;

    result.add_relocatable(offsetof(FunctionDesc, code_ptr), 
                           desc.code_count ? Relocation::Type::LABEL_ADDRESS : Relocation::Type::NULL_ADDRESS, 
                           code_buffer.name());    
    result.add_relocatable(offsetof(FunctionDesc, data_ptr), 
                           desc.data_ptr ? Relocation::Type::LABEL_ADDRESS : Relocation::Type::NULL_ADDRESS, 
                           data_buffer.name());
    result.add_relocatable(offsetof(FunctionDesc, debug_ptr), 
                           desc.debug_count ? Relocation::Type::LABEL_ADDRESS : Relocation::Type::NULL_ADDRESS, 
                           debug_buffer.name());

    // Сохраняем заголовое         
    result.add_bytes(&desc, sizeof(FunctionDesc));

    // Добавляем сгенерированный код после заголовка
    result.add_buffer(code_buffer);
    result.add_buffer(data_buffer);
    result.add_buffer(debug_buffer);

    return result;
}
/**
 * Function compiler
 */
RelocatableBuffer FunctionCompiler::build(FunctionEnv* fe) {
    std::string label_name = fe->get_name();
    return build(fe, label_name);
}


} // namespace sootc