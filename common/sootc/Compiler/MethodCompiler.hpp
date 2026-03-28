#pragma once

#include "common/carbon/files/Definition.hpp"
#include "common/carbon/files/FunctionDesc.hpp"
#include <memory>
#include <vector>

namespace carbon::files {

class MethodBuilder {
public:
    MethodBuilder(const std::string& name);
    
    void set_flags(MethodFlags flags);
    void set_function(std::vector<vm::Instruction> code);
    void set_function(FunctionDesc* function);
    
    std::vector<u8> build();
    
    MethodDef* get_method_def() { return method_def_.get(); }
    
private:
    std::unique_ptr<MethodDef> method_def_;
    std::vector<u8> function_data_;
};

} // namespace carbon::files