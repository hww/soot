// NoneNode.hpp
#pragma once

#include "Node.hpp"

namespace sootc {

class NoneNode : public Node {
public:
    NoneNode() : Node(NodeType::Node) {}
    
    const char* node_type() const override { return "NoneNode"; }
    
    std::string to_string() const override {
        return "none";
    }
    
    ProgramBinaryElement generate(GlobalState& state) override {
        // NoneNode не генерирует код
        (void)state;
        return ProgramBinaryElement(0);
    }
    
    static constexpr NodeType StaticType = NodeType::Node;
};

} // namespace sootc