// src/sootc/Compiler/FunctionCompiler.cpp
#include "common/sootc/Compiler/FunctionCompiler.hpp"
#include "common/carbon/files/BinaryFileBuilder.hpp"

namespace sootc {

FunctionCompiler::FunctionCompiler(TypeSystem &ts, Type *function_type)
    : ts_(ts), function_type_(function_type) {}

IR_Reg *FunctionCompiler::create_local_reg(Type *type) {
    auto    reg = std::make_unique<IR_Reg>(type, next_reg_++, false);
    IR_Reg *ptr = reg.get();
    regs_.push_back(std::move(reg));
    return ptr;
}

IR_Reg *FunctionCompiler::get_self_reg() {
    return new IR_Reg(ts_.lookup_type("object"), IR_Reg::REG_SELF, true);
}

void FunctionCompiler::add_node(std::unique_ptr<IR_Node> node) {
    nodes_.push_back(std::move(node));
}

std::vector<u8> FunctionCompiler::compile() {
    runtime::files::BinaryFileBuilder builder;

    ByteCodeBuilder                     bc_builder;
    std::unordered_map<IR_Value *, u32> reg_map;

    // Строим карту регистров
    for (const auto &reg : regs_) {
        reg_map[reg.get()] = reg->get_index();
    }

    // Генерируем код из всех узлов
    for (const auto &node : nodes_) {
        node->generate(bc_builder, reg_map);
    }

    // Получаем инструкции
    std::vector<Instruction> instructions = bc_builder.get_instructions();

    // Создаем функцию в билдере
    builder.add_function(SID(function_type_->get_name().c_str()), instructions,
                         {}, // data - пока пусто
                         {}  // debug_info - пока пусто
    );

    return builder.build();
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