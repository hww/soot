// src/sootc/Compiler/FunctionCompiler.cpp
#include "common/sootc/Compiler/FunctionCompiler.hpp"
#include "Log.hpp"

namespace sootc {

FunctionCompiler::FunctionCompiler(TypeSystem &ts, Type *function_type, const std::string& func_name)
    : ts_(ts), function_type_(function_type), function_name_(func_name) {}

IR_Reg *FunctionCompiler::create_local_reg(Type *type) {
    auto    reg = std::make_unique<IR_Reg>(type, next_reg_++, false);
    IR_Reg *ptr = reg.get();
    regs_.push_back(std::move(reg));
    return ptr;
}

IR_Reg* FunctionCompiler::create_arg_reg(Type* type, u32 index) {
    auto reg = std::make_unique<IR_Reg>(type, ARG_REGISTERS_OFFSET + index, true);
    IR_Reg* ptr = reg.get();
    regs_.push_back(std::move(reg));
    return ptr;
}

IR_Reg *FunctionCompiler::get_self_reg() {
    auto reg = std::make_unique<IR_Reg>(ts_.lookup_type("object"), IR_Reg::REG_SELF, true);
    IR_Reg* ptr = reg.get();
    regs_.push_back(std::move(reg));
    return ptr;
}

void FunctionCompiler::add_node(std::unique_ptr<IR_Node> node) {
    nodes_.push_back(std::move(node));
}

std::vector<u8> FunctionCompiler::compile() {
    FunctionDescBuilder bc_builder;
    std::unordered_map<IR_Value*, u32> reg_map;

    for (const auto& reg : regs_) {
        reg_map[reg.get()] = reg->get_index();
    }

    for (const auto& node : nodes_) {
        node->generate(bc_builder, reg_map);
    }

    std::vector<Instruction> instructions = bc_builder.get_instructions();
    
    // Конвертируем в байткод
    std::vector<u8> bytecode;
    for (const auto& instr : instructions) {
        const u8* bytes = reinterpret_cast<const u8*>(&instr);
        bytecode.insert(bytecode.end(), bytes, bytes + sizeof(Instruction));
    }
    
    return bytecode;
}

std::string FunctionCompiler::to_string() const {
    std::string result = "FunctionCompiler {\n";
    for (const auto &node : nodes_) {
        result += "  " + node->to_string() + "\n";
    }
    result += "}";
    return result;
}

} // namespace sootc