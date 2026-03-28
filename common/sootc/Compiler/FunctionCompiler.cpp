// src/sootc/Compiler/FunctionCompiler.cpp
#include "common/sootc/Compiler/FunctionCompiler.hpp"
#include "common/carbon/lib/Ptr.hpp"
#include "common/carbon/files/FunctionDesc.hpp"
#include "common/carbon/vm/Instructions.hpp"

#include "Log.hpp"

using namespace carbon::lib;
using namespace carbon::files;
using namespace carbon::vm;

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

RelocatableBuffer FunctionCompiler::compile() {
    FunctionDescBuilder bc_builder;
    std::unordered_map<IR_Value*, u32> reg_map;
    
    for (const auto& reg : regs_) {
        reg_map[reg.get()] = reg->get_index();
    }
    
    for (const auto& node : nodes_) {
        node->generate(bc_builder, reg_map);
    }
    
    std::vector<Instruction> instructions = bc_builder.get_instructions();
    
    RelocatableBuffer buffer;
    
    // Заполняем FunctionDesc
    FunctionDesc desc;
    desc.code_count = static_cast<u32>(instructions.size());
    desc.data_size = 0;
    desc.debug_count = 0;
    desc.reserved = 0;
    desc.owner_module = nullptr;
    desc.code_ptr = nullptr;   // ← nullptr вместо 0
    desc.data_ptr = nullptr;   // ← nullptr вместо 0
    desc.debug_ptr = nullptr;  // ← nullptr вместо 0
    
    // Записываем заголовок
    u32 header_start = buffer.size();
    buffer.add_bytes(&desc, sizeof(FunctionDesc));
    
    // Отмечаем code_ptr как relocatable
    u32 code_ptr_offset = header_start + offsetof(FunctionDesc, code_ptr);
    buffer.add_relocatable_offset(code_ptr_offset);
    
    // Записываем код
    if (!instructions.empty()) {
        u32 code_start = buffer.size();
        buffer.add_bytes(instructions.data(), 
                        instructions.size() * sizeof(Instruction));
        
        // Обновляем code_ptr в заголовке (используем data() для записи)
        Ptr<Instruction>* code_ptr_field = 
            reinterpret_cast<Ptr<Instruction>*>(buffer.data() + code_ptr_offset);
        code_ptr_field->offset = code_start;
    }
    
    return buffer;
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