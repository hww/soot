// common/sootc/Compiler/MethodCompiler.hpp
#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include <string>

using namespace carbon::files;

namespace sootc {

class MethodCompiler {
public:
    MethodCompiler(TypeSystem& ts);
    
    // Компилирует определение метода
    // Пример: (define-method add type ((a int) (b int)) (+ a b))
    RelocatableBuffer compile_method(const script::Object& form, const std::string& method_name);
    
private:
    TypeSystem& ts_;
    
    // Компиляция тела метода в FunctionDesc
    RelocatableBuffer compile_function_body(const script::Object& body, 
                                             const std::vector<std::string>& params);
    
    RelocatableBuffer build_method_buffer(const MethodDef& method_def,
                                           const RelocatableBuffer& function_buffer);
};

} // namespace sootc