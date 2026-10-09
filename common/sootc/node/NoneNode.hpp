// NoneNode.hpp
#pragma once

#include "Node.hpp"
#include <file/ProgramBinaryElement.hpp>

using namespace carbon;

namespace sootc {

class NoneNode : public Node {
public:
    NoneNode() : Node(NodeType::Node) {}
    
    const char* node_type() const override { return "NoneNode"; }
    
    std::string to_string() const override {
        return "none";
    }
    
    ProgramBinaryElement generate(StringsTable& sttings_table) override {
        // NoneNode не генерирует код
        (void)sttings_table;
        return ProgramBinaryElement(0);
    }
    
    static constexpr NodeType StaticType = NodeType::Node;
};

} // namespace sootc