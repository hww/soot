#include "NewNode.hpp"

#include "carbon/lib/StringId.hpp"
#include "type_system/TypeSystem.hpp"
#include "util/Log.hpp"

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

        // ========================================================================
        // emit
        // ========================================================================
        //
        // Runtime allocation of a single object.
        //
        // Step 1. Call `allocate(where, type-id)` — a universal native function
        //         registered by NativeAllocators. It looks the type up in the
        //         GC type registry, takes size and alignment, and returns a
        //         zero-initialized block.
        //
        // Step 2. If the type declares its own `new` method (defmethod new
        //         <type> ...), call it. The constructor receives the allocation
        //         keyword, `this` (the freshly allocated pointer), and the
        //         user-supplied positional arguments, and is responsible for
        //         filling in non-zero fields. If there is no constructor, the
        //         zero-initialized block is the final value.
        //
        void NewNode::emit(FunctionNode &fn) {
            if (is_static()) {
                throw std::runtime_error(
                    "NewNode::emit: 'static' initialization is only valid at top level "
                    "(inside (define ...)); use 'global' or 'heap' for runtime allocation");
            }

            const std::string &type_name = m_type.base_type();

            // -------- Step 1: allocate(where, type-id) --------
            u8 alloc_fn_reg = fn.alloc_temp_reg(nullptr);
            {
                u16 st = fn.add_constant(StringId("allocate").value, FunctionNode::ConstKind::SID);
                fn.add_instruction_imm_u16(Opcode::LookupPointer, alloc_fn_reg, st);
            }

            // Argument 0: allocation keyword, as a SID.
            {
                u8  reg = fn.alloc_temp_reg(nullptr);
                u16 st =
                    fn.add_constant(StringId(m_allocation).value, FunctionNode::ConstKind::SID);
                fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, reg, st);
                fn.add_instruction(Opcode::Move, ARG_REGISTERS_OFFSET + 0, reg, 0);
            }

            // Argument 1: type SID.
            {
                u8  reg = fn.alloc_temp_reg(nullptr);
                u16 st = fn.add_constant(StringId(type_name).value, FunctionNode::ConstKind::SID);
                fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, reg, st);
                fn.add_instruction(Opcode::Move, ARG_REGISTERS_OFFSET + 1, reg, 0);
            }

            u8 ptr_reg = fn.alloc_temp_reg(nullptr);
            fn.add_instruction(Opcode::CallFf, ptr_reg, alloc_fn_reg, 2);

            // -------- Step 2: optional constructor --------
            //
            // The compiler asks the TypeSystem whether the type declares its
            // *own* `new` method (not one inherited from an ancestor). A type
            // without its own `new` simply produces a zeroed instance.
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
            {
                u16 st = fn.add_constant(StringId(ctor_name).value, FunctionNode::ConstKind::SID);
                fn.add_instruction_imm_u16(Opcode::LookupPointer, ctor_reg, st);
            }

            std::vector<u8> arg_regs;

            // arg 0: allocation keyword
            {
                u8  reg = fn.alloc_temp_reg(nullptr);
                u16 st =
                    fn.add_constant(StringId(m_allocation).value, FunctionNode::ConstKind::SID);
                fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, reg, st);
                arg_regs.push_back(reg);
            }

            // arg 1: this (the freshly allocated pointer)
            arg_regs.push_back(ptr_reg);

            // args 2..N: user arguments
            //
            // For 'static' the fields are keyword-args (`:x 1.0`). For 'heap'
            // the user writes either positional args (`1.0 2.0 3.0 4.0`) or
            // keyword-args (`:x 1.0 :y 2.0 ...`). Both are accepted here: if
            // m_fields is non-empty, its values are emitted in declaration
            // order; otherwise m_args is used.
            if (!m_fields.empty()) {
                for (const auto &f : m_fields) {
                    if (!f.value) continue;
                    f.value->emit(fn);
                    arg_regs.push_back(fn.get_temp_reg(f.value.get()));
                }
            } else {
                for (auto &a : m_args) {
                    a->emit(fn);
                    arg_regs.push_back(fn.get_temp_reg(a.get()));
                }
            }

            for (size_t i = 0; i < arg_regs.size(); ++i) {
                fn.add_instruction(Opcode::Move, static_cast<u8>(ARG_REGISTERS_OFFSET + i),
                                   arg_regs[i], 0);
            }

            u8 ret_reg = fn.alloc_temp_reg(nullptr);
            fn.add_instruction(Opcode::Call, ret_reg, ctor_reg, static_cast<u8>(arg_regs.size()));
            fn.set_temp_reg(this, ret_reg);
        }

        // ========================================================================
        // generate
        // ========================================================================
        //
        // Static initialization is handled by DataDeclarationNode; a bare
        // NewNode never produces a binary on its own.
        //
        ProgramBinaryElement NewNode::generate(StringsTable &state) {
            (void)state;
            return ProgramBinaryElement(0);
        }



} // namespace sootc