// common/sootc/Compiler/StateCompiler.hpp
#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include <string>

using namespace carbon::files;

namespace sootc {

class StateCompiler {
public:
    StateCompiler(TypeSystem& ts);
    
    // Компилирует определение состояния
    // Пример: (define-state Running (parent State) 
    //          (on-enter ...) (on-exit ...) (on-event ...))
    RelocatableBuffer compile_state(const script::Object& form, const std::string& state_name);
    
private:
    TypeSystem& ts_;
    
    // Компиляция обработчиков (enter, exit, event, code, trans, post)
    std::vector<RelocatableBuffer> compile_handlers(const script::Object& handlers_form);
    
    RelocatableBuffer build_state_buffer(const StateDesc& state_desc,
                                          const std::vector<Definition>& handlers);
};

} // namespace sootc