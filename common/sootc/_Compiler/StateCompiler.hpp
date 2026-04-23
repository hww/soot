// common/sootc/Compiler/StateCompiler.hpp
#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/soot/Object.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include "sootc/Env/StateEnv.hpp"
#include "sootc/IR/IR_Value.hpp"
#include <string>

using namespace carbon;

namespace sootc {

class StateCompiler {
public:
    StateCompiler(TypeSystem& ts, Compiler* compiler);
    
    // Компилирует определение состояния
    // Пример: (defstate name (parent) :virtual #t :event ... :enter ... :exit ... :code ... :post ...)
    IR_Value* compile(const soot::Object& form, const soot::Object& rest, Env* env);
    
    // BUILD Phase
    ProgramBinaryElement build(StateEnv* s_env);

private:
    struct HandlerForms {
        soot::Object event;
        soot::Object enter;
        soot::Object exit;
        soot::Object code;
        soot::Object post;
        soot::Object trans;
        bool is_virtual = false;
    };
    
    std::string extract_state_name(const soot::Object& form, const soot::Object& rest);
    std::string extract_parent_name(const soot::Object& form, const soot::Object& rest);
    HandlerForms extract_handlers(const soot::Object& form, const soot::Object& rest);
    void compile_handler(const soot::Object& handler_form, const std::string& handler_name, 
                         StateEnv* s_env, MethodEnv*& out_method_env);
    void compile_handler_with_signature(
    const soot::Object& handler_form,
    const std::string& handler_name,
    StateEnv* s_env,
    const TypeSpec& expected_signature,
    MethodEnv*& out_method_env);

    TypeSystem& ts_;
    Compiler* compiler_;
};

} // namespace sootc