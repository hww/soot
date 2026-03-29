// common/sootc/Compiler/TypeCompiler.hpp
#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "type_system/Type.hpp"
#include <string>
#include <vector>

using namespace carbon::files;

namespace sootc {

class TypeCompiler {
public:
    TypeCompiler(TypeSystem& ts);
    
    // Компилирует определение типа в RelocatableBuffer
    // Пример формы: (define-type MyClass (parent Object) 
    //                (methods (add ...) (sub ...))
    //                (states (Running ...) (Stopped ...)))
    RelocatableBuffer compile_type(const script::Object& form, EnvironmentMap* constance = nullptr);
    
private:
    TypeSystem& ts_;
    
    // Компиляция методов типа
    std::vector<MethodDef> compile_methods(const script::Object& methods_form);
    
    // Компиляция состояний типа
    std::vector<StateDef> compile_states(const script::Object& states_form);
    
    // Сборка TypeDesc с методами и состояниями
    RelocatableBuffer build_type_buffer(const TypeDesc& type_desc,
                                        const std::vector<MethodDef>& methods,
                                        const std::vector<StateDef>& states);
};

} // namespace sootc