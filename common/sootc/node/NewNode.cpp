#include "NewNode.hpp"

#include "common/carbon/lib/StringId.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/util/Log.hpp"
#include <algorithm>

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

        // =====================================================================
        // Step 1: allocate.
        //
        // Call `allocate-<type>(allocation)` — a native function registered
        // by NativeAllocators. It returns a pointer to a zero-initialized
        // slot large enough for one <type>.
        // =====================================================================
        const std::string alloc_name = "allocate-" + type_name;

        u8  alloc_reg = fn.alloc_temp_reg(nullptr);
        u16 alloc_st = fn.add_constant(StringId(alloc_name).value, FunctionNode::ConstKind::SID);
        fn.add_instruction_imm_u16(Opcode::LookupPointer, alloc_reg, alloc_st);

        // Argument 0: allocation keyword, as a SID.
        {
            u8  reg = fn.alloc_temp_reg(nullptr);
            u16 st = fn.add_constant(StringId(m_allocation).value, FunctionNode::ConstKind::SID);
            fn.add_instruction_imm_u16(Opcode::LookupInt, reg, st);
            fn.add_instruction(Opcode::Move, ARG_REGISTERS_OFFSET + 0, reg, 0);
        }

        // Call the allocator. It is a native function, so CallFf.
        u8 ptr_reg = fn.alloc_temp_reg(nullptr);
        fn.add_instruction(Opcode::CallFf, ptr_reg, alloc_reg, 1);

        // =====================================================================
        // Step 2: construct, only if the type has its own `new` method.
        //
        // `get_my_new_method` returns true only when `(defmethod new <type>
        // ...)` was declared *for this type*, not for an ancestor. A type
        // without its own `new` simply produces a zeroed instance, which is
        // the correct behaviour for GOAL-style types.
        // =====================================================================
        bool has_ctor = false;
        if (Type *t = TypeSystem::instance().lookup_type_no_throw(type_name)) {
            MethodInfo info;
            has_ctor = t->get_my_new_method(&info);
        }

        if (!has_ctor) {
            fn.set_temp_reg(this, ptr_reg);
            return;
        }

        const std::string ctor_name = type_name + "-new";
        u8                ctor_reg = fn.alloc_temp_reg(nullptr);
        u16 ctor_st = fn.add_constant(StringId(ctor_name).value, FunctionNode::ConstKind::SID);
        fn.add_instruction_imm_u16(Opcode::LookupPointer, ctor_reg, ctor_st);

        std::vector<u8> arg_regs;

        // 2a. allocation keyword (SID).
        {
            u8  reg = fn.alloc_temp_reg(nullptr);
            u16 st = fn.add_constant(StringId(m_allocation).value, FunctionNode::ConstKind::SID);
            fn.add_instruction_imm_u16(Opcode::LookupInt, reg, st);
            arg_regs.push_back(reg);
        }

        // 2b. `this` — the freshly allocated pointer.
        arg_regs.push_back(ptr_reg);

        // 2c. User arguments (positional for non-static).
        for (auto &a : m_args) {
            a->emit(fn);
            arg_regs.push_back(fn.get_temp_reg(a.get()));
        }

        // Move everything into r24+.
        for (size_t i = 0; i < arg_regs.size(); ++i) {
            fn.add_instruction(Opcode::Move, static_cast<u8>(ARG_REGISTERS_OFFSET + i), arg_regs[i],
                               0);
        }

        // Call the constructor. It is a ScriptLambda, so Call, not CallFf.
        u8 ret_reg = fn.alloc_temp_reg(nullptr);
        fn.add_instruction(Opcode::Call, ret_reg, ctor_reg, static_cast<u8>(arg_regs.size()));
        fn.set_temp_reg(this, ret_reg);
    }

    ProgramBinaryElement NewNode::generate(StringsTable &state) {
        // Static initialization is handled by DataDeclarationNode, which
        // reads the parsed fields and bakes them into a data-instance entry.
        // A bare NewNode never generates a binary on its own.
        (void)state;
        return ProgramBinaryElement(0);
    }

} // namespace sootc