#include "MethodCallNode.hpp"
#include "FunctionNode.hpp"

#include "carbon/vm/Instructions.hpp"
#include "carbon/lib/StringId.hpp"
#include "util/Log.hpp"

namespace sootc {

    MethodCallNode::MethodCallNode(std::unique_ptr<ExpressionNode> obj, std::string full_name,
                                   std::vector<std::unique_ptr<ExpressionNode>> args,
                                   Type                                        *return_type)
        : ExpressionNode(return_type), m_obj(std::move(obj)), m_full_name(std::move(full_name)),
          m_args(std::move(args)) {
        adopt(m_obj);
        adopt_all(m_args);
    }

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
        // Virtual dispatch: the receiver is a basic-derived object whose
        // first 8 bytes hold the SID64 of its runtime type. Emit CallMethod
        // so the VM resolves the SsType* and indexes SsType::m_pMethods
        // by m_method_id at runtime.
        //
        // Static dispatch: the type is known at compile time and the
        // method has no virtual semantics. Emit LookupPointer + Call as
        // before.
        //
        // m_method_id is set by NodeBuilder::build_deref from MethodInfo.id
        // when the type is basic and the method is virtual
        // (TypeSystem::should_use_virtual_methods). When it is -1, we fall
        // back to the static path.
        if (m_method_id >= 0) {
            // ---- 1. Evaluate receiver + args into temp registers. ----
            std::vector<u8> arg_regs;
            m_obj->emit(fn);
            arg_regs.push_back(fn.get_temp_reg(m_obj.get()));
            for (auto &arg : m_args) {
                arg->emit(fn);
                arg_regs.push_back(fn.get_temp_reg(arg.get()));
            }

            // ---- 2. Move into r24+ (receiver first, then args). ----
            for (size_t i = 0; i < arg_regs.size(); ++i) {
                fn.add_instruction(Opcode::Move, static_cast<u8>(ARG_REGISTERS_OFFSET + i),
                                   arg_regs[i], 0);
            }

            // ---- 3. Load method_id into a temp register. ----
            u8  idx_reg = fn.alloc_temp_reg(nullptr);
            u16 idx_st =
                fn.add_constant(static_cast<u64>(m_method_id), FunctionNode::ConstKind::INT);
            fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, idx_reg, idx_st);

            // ---- 4. CallMethod: a = dst, b = method_id reg, c = argc. ----
            u8 ret_reg = fn.alloc_temp_reg(m_type);
            fn.add_instruction(Opcode::CallMethod, ret_reg, idx_reg,
                               static_cast<u8>(arg_regs.size()));
            fn.set_temp_reg(this, ret_reg);
            return;
        }

        // ---- Static path (unchanged). ----
        u8  fn_reg = fn.alloc_temp_reg(nullptr);
        u16 st_idx = fn.add_constant(StringId(m_full_name).value, FunctionNode::ConstKind::SID);
        fn.add_instruction_imm_u16(Opcode::LookupPointer, fn_reg, st_idx);

        std::vector<u8> arg_regs;
        m_obj->emit(fn);
        arg_regs.push_back(fn.get_temp_reg(m_obj.get()));
        for (auto &arg : m_args) {
            arg->emit(fn);
            arg_regs.push_back(fn.get_temp_reg(arg.get()));
        }

        for (size_t i = 0; i < arg_regs.size(); ++i) {
            fn.add_instruction(Opcode::Move, static_cast<u8>(ARG_REGISTERS_OFFSET + i), arg_regs[i],
                               0);
        }

        const Opcode call_op = m_is_native ? Opcode::CallFf : Opcode::Call;

        u8 ret_reg = fn.alloc_temp_reg(m_type);
        fn.add_instruction(call_op, ret_reg, fn_reg, static_cast<u8>(arg_regs.size()));

        fn.set_temp_reg(this, ret_reg);
    }

} // namespace sootc