#pragma once

#include "Node.hpp"
#include "ExpressionNode.hpp"
#include <vector>

namespace sootc {

class CallNode : public ExpressionNode {
    std::string m_function_name;
    std::vector<std::unique_ptr<ExpressionNode>> m_args;
    
public:
    CallNode(const std::string& name, Type* return_type)
        : ExpressionNode(NodeType::CallNode, return_type), m_function_name(name) {}
    
    void add_argument(std::unique_ptr<ExpressionNode> arg) {
        m_args.push_back(std::move(arg));
    }
    
    void emit(FunctionNode& fn) override {
        for (auto& arg : m_args) {
            arg->emit(fn);
        }
        
        u8 func_reg = fn.get_variable_reg(m_function_name);
        
        u8 result_reg = fn.alloc_temp_reg(m_type);
        fn.add_instruction(Opcode::Call, result_reg, func_reg, 
                          static_cast<u8>(m_args.size()));
        fn.set_temp_reg(this, result_reg);
    }
    
    std::string to_string() const override {
        std::string result = "(call " + m_function_name;
        for (auto& arg : m_args) {
            result += " " + arg->to_string();
        }
        return result + ")";
    }
};

} // namespace sootc