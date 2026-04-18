#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include "sootc/IR/IR_Value.hpp"

using  namespace carbon;
using  namespace carbon;

namespace sootc {

class MethodCompiler {
public:
    MethodCompiler(TypeSystem& ts, Compiler* compiler);
    
    // Единый метод компиляции метода
    IR_Value* compile(const script::Object& form, const script::Object& rest, Env* env);
    
    // BUILD Phase
    ProgramBinaryElement build(MethodEnv* m_env);

private:
    std::string extract_method_name(const script::Object& form);
    void parse_form(const script::Object& rest, 
                    std::string& out_type_name,
                    script::Object& out_args_list,
                    script::Object& out_body_forms);


    TypeSystem& ts_;
    Compiler* compiler_;
};

} // namespace sootc
