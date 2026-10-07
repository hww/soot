#include "NewNode.hpp"

#include "common/carbon/lib/StringId.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/util/Log.hpp"

namespace sootc {

    std::string NewNode::to_string() const {
        std::string result = "(new " + m_allocation + " " + m_type.print();
        for (const auto &f : m_fields) {
            result += " :" + f.name + " " + (f.value ? f.value->to_string() : "none");
        }
        for (const auto &a : m_args) { result += " " + (a ? a->to_string() : "none"); }
        return result + ")";
    }

    /// @brief Emit code for a constructor call.
    /// @details See the header comment in NewNode.hpp.
    void NewNode::emit(FunctionNode &fn) {
        if (is_static()) {
            throw std::runtime_error("NewNode::emit: 'static' initialization is only valid at top "
                                     "level (inside (define ...)); use 'global'/'heap'/'stack' for "
                                     "runtime allocation");
        }

        const std::string &type_name = m_type.base_type();

        // 1. Resolve the constructor "<type>-new".
        const std::string ctor_name = type_name + "-new";

        u8  ctor_reg = fn.alloc_temp_reg(nullptr);
        u16 ctor_st = fn.add_constant(StringId(ctor_name).value, FunctionNode::ConstKind::INT);
        fn.add_instruction_imm_u16(Opcode::LookupPointer, ctor_reg, ctor_st);

        // 2. Build the argument list: allocation SID, type-to-make SID,
        //    then user arguments.
        std::vector<u8> arg_regs;

        // 2a. allocation SID (e.g. "global").
        {
            u8  reg = fn.alloc_temp_reg(nullptr);
            u16 st = fn.add_constant(StringId(m_allocation).value, FunctionNode::ConstKind::INT);
            fn.add_instruction_imm_u16(Opcode::LookupInt, reg, st);
            arg_regs.push_back(reg);
        }

        // 2b. type-to-make SID (e.g. "vec3").
        {
            u8  reg = fn.alloc_temp_reg(nullptr);
            u16 st = fn.add_constant(StringId(type_name).value, FunctionNode::ConstKind::INT);
            fn.add_instruction_imm_u16(Opcode::LookupInt, reg, st);
            arg_regs.push_back(reg);
        }

        // 2c. User arguments (positional).
        for (auto &a : m_args) {
            a->emit(fn);
            arg_regs.push_back(fn.get_temp_reg(a.get()));
        }

        // 3. Move into r24+.
        for (size_t i = 0; i < arg_regs.size(); ++i) {
            fn.add_instruction(Opcode::Move, static_cast<u8>(ARG_REGISTERS_OFFSET + i), arg_regs[i],
                               0);
        }

        // 4. Call and store result.
        u8 ret_reg = fn.alloc_temp_reg(nullptr);
        fn.add_instruction(Opcode::Call, ret_reg, ctor_reg, static_cast<u8>(arg_regs.size()));
        fn.set_temp_reg(this, ret_reg);
    }

    ProgramBinaryElement NewNode::generate(GlobalState &state) {
        // Static initialization is handled by DataDeclarationNode, which
        // reads the parsed fields and bakes them into a data-instance entry.
        // A bare NewNode never generates a binary on its own.
        (void)state;
        return ProgramBinaryElement(0);
    }

} // namespace sootc