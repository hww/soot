// common/sootc/Compiler/StateCompiler.hpp
#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "sootc/Env/StateEnv.hpp"
#include "sootc/IR/IR_Value.hpp"
#include <string>

using namespace carbon::files;

namespace sootc {

class StateCompiler {
public:
    StateCompiler(TypeSystem& ts, Compiler* compiler);
    
    // Компилирует определение состояния
    // Пример: (defstate name (parent) :virtual #t :event ... :enter ... :exit ... :code ... :post ...)
    IR_Value* compile(const script::Object& form, const script::Object& rest, Env* env);
    
    // BUILD Phase
    RelocatableBuffer build(StateEnv* s_env);

private:
    struct HandlerForms {
        script::Object event;
        script::Object enter;
        script::Object exit;
        script::Object code;
        script::Object post;
        script::Object trans;
        bool is_virtual = false;
    };
    
    std::string extract_state_name(const script::Object& form);
    std::string extract_parent_name(const script::Object& rest);
    HandlerForms extract_handlers(const script::Object& rest);
    void compile_handler(const script::Object& handler_form, const std::string& handler_name, 
                         StateEnv* s_env, MethodEnv*& out_method_env);
    
    TypeSystem& ts_;
    Compiler* compiler_;
};

} // namespace sootc