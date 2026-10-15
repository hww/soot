#include "SetNode.hpp"
#include "FunctionNode.hpp"


#include "carbon/vm/Instructions.hpp"
#include "util/Log.hpp"

namespace sootc {

    std::string SetNode::to_string() const {
        std::string target;
        if (m_at_lvalue) {
            target = m_at_lvalue->to_string();
        } else if (m_lvalue) {
            target = m_lvalue->to_string();
        } else {
            target = m_name;
        }
        return "(set! " + target + " " + m_value->to_string() + ")";
    }

    /// @brief Emit code for (set! target value).
    /// @details For a variable target: evaluate the value and Move it into the
    ///          variable's register. For a field target: compute the field
    ///          address with DerefNode::emit_lvalue, then Store into it.
    void SetNode::emit(FunctionNode &fn) {
        if (!m_value) { throw std::runtime_error("SetNode::emit: no value"); }

        // ---- 1. Evaluate the value. ----
        m_value->emit(fn);
        u8 value_reg = fn.get_temp_reg(m_value.get());

        // ---- 2a. Variable target. ----
        if (!m_lvalue && !m_at_lvalue) {
            auto *info = fn.lookup_variable(m_name);
            if (!info) {
                throw std::runtime_error("SetNode::emit: undefined variable '" + m_name + "'");
            }
            u8 local_reg = info->reg();
            if (local_reg != value_reg) {
                fn.add_instruction(Opcode::Move, local_reg, value_reg, 0);
            }
            fn.set_temp_reg(this, value_reg);
            m_type = m_value->get_type();
            return;
        }

        // ---- 2b'. At-target: (set! (at arr i) v) ----
        if (m_at_lvalue) {
            Type *elem_type = m_at_lvalue->elem_type();
            if (!elem_type) {
                throw std::runtime_error("SetNode::emit: at-target has no element type");
            }

            // Compute the element address.
            u8 addr_reg = m_at_lvalue->emit_lvalue(fn);

            // Pick the Store* opcode from the element type.
            Opcode store_op;
            if (elem_type->is_reference()) {
                store_op = Opcode::StorePointer;
            } else if (elem_type->get_preferred_reg_class() == RegClass::FPR) {
                store_op = Opcode::StoreFloat;
            } else {
                switch (elem_type->get_load_size()) {
                case 1: store_op = Opcode::StoreI8; break;
                case 2: store_op = Opcode::StoreI16; break;
                case 4: store_op = Opcode::StoreI32; break;
                case 8: store_op = Opcode::StoreI64; break;
                default:
                    throw std::runtime_error(fmt::format(
                        "SetNode::emit: unsupported at-store size {}", elem_type->get_load_size()));
                }
            }

            // Store* convention: a = address, b = value.
            fn.add_instruction(store_op, addr_reg, value_reg, 0);

            fn.set_temp_reg(this, value_reg);
            m_type = elem_type;
            return;
        }

        // ---- 2b. Field target (existing) ----
        Type *field_type = m_lvalue->field_type();
        if (!field_type) { throw std::runtime_error("SetNode::emit: field target has no type"); }

        u8 addr_reg = m_lvalue->emit_lvalue(fn);

        Opcode store_op;
        if (field_type->is_reference()) {
            store_op = Opcode::StorePointer;
        } else if (field_type->get_preferred_reg_class() == RegClass::FPR) {
            store_op = Opcode::StoreFloat;
        } else {
            switch (field_type->get_load_size()) {
            case 1: store_op = Opcode::StoreI8; break;
            case 2: store_op = Opcode::StoreI16; break;
            case 4: store_op = Opcode::StoreI32; break;
            case 8: store_op = Opcode::StoreI64; break;
            default:
                throw std::runtime_error(fmt::format(
                    "SetNode::emit: unsupported field store size {}", field_type->get_load_size()));
            }
        }

        fn.add_instruction(store_op, addr_reg, value_reg, 0);
        fn.set_temp_reg(this, value_reg);
        m_type = field_type;
    }

} // namespace sootc