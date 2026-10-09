#include "MethodCallNode.hpp"
#include "FunctionNode.hpp"

#include "carbon/vm/Instructions.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/util/Log.hpp"

namespace sootc {

    MethodCallNode::MethodCallNode(std::unique_ptr<ExpressionNode> obj, std::string full_name,
                                   std::vector<std::unique_ptr<ExpressionNode>> args,
                                   Type                                        *return_type)
        : ExpressionNode(return_type), m_obj(std::move(obj)), m_full_name(std::move(full_name)),
          m_args(std::move(args)) {}

    std::string MethodCallNode::to_string() const {
        std::string s = "(-> " + m_obj->to_string() + " " + m_full_name;
        for (auto &a : m_args) { s += " " + a->to_string(); }
        return s + ")";
    }

    /// @brief Emit a direct method call.
    /// @details 1. Look up the method's ScriptLambda by its full name.
    ///          2. Evaluate the receiver and arguments into temp registers.
    ///          3. Move them into the argument registers (r24+).
    ///          4. Emit the call and store the result in a new temp register.
    void MethodCallNode::emit(FunctionNode &fn) {
        // 1. Resolve the method lambda from the symbol table.
        u8  fn_reg = fn.alloc_temp_reg(nullptr);
        u16 st_idx = fn.add_constant(StringId(m_full_name).value, FunctionNode::ConstKind::SID);
        fn.add_instruction_imm_u16(Opcode::LookupPointer, fn_reg, st_idx);

        // 2. Evaluate receiver + arguments.
        std::vector<u8> arg_regs;
        m_obj->emit(fn);
        arg_regs.push_back(fn.get_temp_reg(m_obj.get()));
        for (auto &arg : m_args) {
            arg->emit(fn);
            arg_regs.push_back(fn.get_temp_reg(arg.get()));
        }

        // 3. Move values into the argument registers (r24+).
        for (size_t i = 0; i < arg_regs.size(); ++i) {
            fn.add_instruction(Opcode::Move, static_cast<u8>(ARG_REGISTERS_OFFSET + i), arg_regs[i],
                               0);
        }

        // 4. Call — CallFf for native methods, Call for script methods.
        const Opcode call_op = m_is_native ? Opcode::CallFf : Opcode::Call;

        u8 ret_reg = fn.alloc_temp_reg(m_type);
        fn.add_instruction(call_op, ret_reg, fn_reg, static_cast<u8>(arg_regs.size()));

        fn.set_temp_reg(this, ret_reg);
    }

} // namespace sootc