#include "NewNode.hpp"
#include "FunctionNode.hpp"

#include "carbon/lib/StringId.hpp"
#include "carbon/vm/Instructions.hpp"
#include "type_system/Type.hpp"
#include "type_system/TypeSystem.hpp"
#include "util/Log.hpp"

#include <algorithm>
#include <cstring>

namespace sootc {

    namespace {

        // Return the store opcode for a scalar field of the given type.
        // The value in the register must already be the right width:
        // LoadInt/LoadFloat from a Constant already produces a 64-bit
        // integer or double; the Store* opcode truncates.
        Opcode store_op_for(Type *field_type) {
            if (field_type->get_preferred_reg_class() == RegClass::FPR) {
                return Opcode::StoreFloat;
            }
            switch (field_type->get_load_size()) {
            case 1: return Opcode::StoreI8;
            case 2: return Opcode::StoreI16;
            case 4: return Opcode::StoreI32;
            case 8: return Opcode::StoreI64;
            default:
                throw std::runtime_error(fmt::format("NewNode: unsupported scalar store size {}",
                                                     field_type->get_load_size()));
            }
        }

    } // namespace

    void NewNode::emit(FunctionNode &fn) {
        if (is_static()) {
            throw std::runtime_error(
                "NewNode::emit: 'static' initialization is only valid at top level "
                "(inside (define ...)); use 'global' or 'heap' for runtime allocation");
        }
        // =====================================================================
        // Heap inline-array
        // =====================================================================
        //
        // (new 'heap 'inline-array T N e0 e1 ... eK)
        //
        // The result is a raw block of elem_size * N bytes in the GC heap.
        // We do NOT call <T>-new: the elements are anonymous values, not
        // structure instances with methods. We allocate the block with
        // allocate-bytes(total_size, align) and write each element into
        // its own slot.
        //
        // Limitation (first step): only scalar element types are supported.
        // Structural elements (vector, matrix, ...) would require expanding
        // each element's subfields at runtime, which is a later step. If the
        // element type is a reference type, we throw a clear error here
        // rather than silently producing corrupt memory.
        if (m_is_inline_array) {
            TypeSystem &ts = TypeSystem::instance();
            Type       *elem_type = ts.lookup_type_no_throw(m_elem_type_name);
            if (!elem_type) {
                throw std::runtime_error(fmt::format(
                    "NewNode::emit: inline-array element type '{}' is unknown", m_elem_type_name));
            }

            const u32 elem_size = static_cast<u32>(elem_type->get_size_in_memory());
            const u32 elem_align = static_cast<u32>(elem_type->get_in_memory_alignment());

            if (elem_size == 0 || m_array_size == 0) {
                throw std::runtime_error(fmt::format("NewNode::emit: inline-array has zero size "
                                                     "(elem_size={}, count={})",
                                                     elem_size, m_array_size));
            }

            const u64 total_size = static_cast<u64>(elem_size) * m_array_size;

            // -------- allocate-bytes(total_size, elem_align) --------

            u8 alloc_fn_reg = fn.alloc_temp_reg(nullptr);
            {
                u16 st =
                    fn.add_constant(StringId("allocate-bytes").value, FunctionNode::ConstKind::SID);
                fn.add_instruction_imm_u16(Opcode::LookupPointer, alloc_fn_reg, st);
            }

            // arg 0: size
            {
                u8  reg = fn.alloc_temp_reg(nullptr);
                u16 st = fn.add_constant(total_size, FunctionNode::ConstKind::INT);
                fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, reg, st);
                fn.add_instruction(Opcode::Move, ARG_REGISTERS_OFFSET + 0, reg, 0);
            }

            // arg 1: align
            {
                u8  reg = fn.alloc_temp_reg(nullptr);
                u16 st =
                    fn.add_constant(static_cast<u64>(elem_align), FunctionNode::ConstKind::INT);
                fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, reg, st);
                fn.add_instruction(Opcode::Move, ARG_REGISTERS_OFFSET + 1, reg, 0);
            }

            u8 ptr_reg = fn.alloc_temp_reg(nullptr);
            fn.add_instruction(Opcode::CallFf, ptr_reg, alloc_fn_reg, 2);

           // -------- write each element into its slot --------

            const auto &elems = m_elements;

            if (elem_type->is_reference()) {
                // Structural element. The element type must be a
                // StructureType; we expand each field of each element.
                auto *st = dynamic_cast<StructureType *>(elem_type);
                if (!st) {
                    throw std::runtime_error(
                        fmt::format("NewNode::emit: inline-array element type '{}' is a "
                                    "reference but not a structure",
                                    m_elem_type_name));
                }

                for (size_t i = 0; i < elems.size(); ++i) {
                    if (!elems[i]) { continue; }

                    auto *elem_new = dynamic_cast<const NewNode *>(elems[i].get());
                    if (!elem_new) {
                        throw std::runtime_error(
                            fmt::format("NewNode::emit: element {} of inline-array '{}' "
                                        "is not a (new ...) form",
                                        i, m_elem_type_name));
                    }

                    const u64 elem_base = static_cast<u64>(i) * elem_size;

                    for (const auto &field : st->fields()) {
                        const NewNode::FieldInit *init = nullptr;
                        for (const auto &fi : elem_new->fields()) {
                            if (fi.name == field.name()) {
                                init = &fi;
                                break;
                            }
                        }
                        if (!init || !init->value) { continue; }

                        Type *field_type = ts.lookup_type_no_throw(field.type().base_type());
                        if (!field_type) { continue; }

                        if (field_type->is_reference()) {
                            throw std::runtime_error(
                                fmt::format("NewNode::emit: field '{}' of inline-array element "
                                            "has reference type '{}', not yet supported",
                                            field.name(), field.type().print()));
                        }

                        init->value->emit(fn);
                        u8 value_reg = fn.get_temp_reg(init->value.get());

                        const u64 addr_off = elem_base + static_cast<u64>(field.offset());

                        u8 addr_reg = fn.alloc_temp_reg(nullptr);
                        if (addr_off <= 0xFF) {
                            fn.add_instruction_imm_reg(Opcode::IAddImm, addr_reg, ptr_reg,
                                                       static_cast<u8>(addr_off));
                        } else {
                            u8  off_reg = fn.alloc_temp_reg(nullptr);
                            u16 st_idx = fn.add_constant(addr_off, FunctionNode::ConstKind::INT);
                            fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, off_reg, st_idx);
                            fn.add_instruction(Opcode::IAdd, addr_reg, ptr_reg, off_reg);
                        }

                        const Opcode store_op = store_op_for(field_type);
                        fn.add_instruction(store_op, addr_reg, value_reg, 0);
                    }
                }

                fn.set_temp_reg(this, ptr_reg);
                return;
            }

            // ---- Scalar elements (as before) ----
            const Opcode store_op = store_op_for(elem_type);

            for (size_t i = 0; i < elems.size(); ++i) {
                if (!elems[i]) { continue; }

                elems[i]->emit(fn);
                u8 value_reg = fn.get_temp_reg(elems[i].get());

                const u64 offset = static_cast<u64>(i) * elem_size;

                u8 addr_reg = fn.alloc_temp_reg(nullptr);

                if (offset <= 0xFF) {
                    fn.add_instruction_imm_reg(Opcode::IAddImm, addr_reg, ptr_reg,
                                               static_cast<u8>(offset));
                } else {
                    u8  off_reg = fn.alloc_temp_reg(nullptr);
                    u16 st = fn.add_constant(offset, FunctionNode::ConstKind::INT);
                    fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, off_reg, st);
                    fn.add_instruction(Opcode::IAdd, addr_reg, ptr_reg, off_reg);
                }

                fn.add_instruction(store_op, addr_reg, value_reg, 0);
            }

            fn.set_temp_reg(this, ptr_reg);
            return;
        }
        const std::string &type_name = m_type.base_type();
        Type              *type = TypeSystem::instance().lookup_type_no_throw(type_name);
        if (!type) {
            throw std::runtime_error(fmt::format("NewNode::emit: unknown type '{}'", type_name));
        }

        // -------- Step 1: allocate(where, type-id) --------
        u8 alloc_fn_reg = fn.alloc_temp_reg(nullptr);
        {
            u16 st = fn.add_constant(StringId("allocate").value, FunctionNode::ConstKind::SID);
            fn.add_instruction_imm_u16(Opcode::LookupPointer, alloc_fn_reg, st);
        }

        {
            u8  reg = fn.alloc_temp_reg(nullptr);
            u16 st = fn.add_constant(StringId(m_allocation).value, FunctionNode::ConstKind::SID);
            fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, reg, st);
            fn.add_instruction(Opcode::Move, ARG_REGISTERS_OFFSET + 0, reg, 0);
        }
        {
            u8  reg = fn.alloc_temp_reg(nullptr);
            u16 st = fn.add_constant(StringId(type_name).value, FunctionNode::ConstKind::SID);
            fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, reg, st);
            fn.add_instruction(Opcode::Move, ARG_REGISTERS_OFFSET + 1, reg, 0);
        }

        u8 ptr_reg = fn.alloc_temp_reg(nullptr);
        fn.add_instruction(Opcode::CallFf, ptr_reg, alloc_fn_reg, 2);

        // -------- Step 2: does the type have its own constructor? --------
        bool has_ctor = false;
        if (Type *t = TypeSystem::instance().lookup_type_no_throw(type_name)) {
            MethodInfo info;
            has_ctor = t->get_my_new_method(&info);
        }

        if (has_ctor) {
            // Existing path: call <type>-new(alloc, this, args...).
            const std::string ctor_name = type_name + "-new";
            u8                ctor_reg = fn.alloc_temp_reg(nullptr);
            {
                u16 st = fn.add_constant(StringId(ctor_name).value, FunctionNode::ConstKind::SID);
                fn.add_instruction_imm_u16(Opcode::LookupPointer, ctor_reg, st);
            }

            std::vector<u8> arg_regs;
            {
                u8  reg = fn.alloc_temp_reg(nullptr);
                u16 st =
                    fn.add_constant(StringId(m_allocation).value, FunctionNode::ConstKind::SID);
                fn.add_instruction_imm_u16(Opcode::LoadStaticI64Imm, reg, st);
                arg_regs.push_back(reg);
            }
            arg_regs.push_back(ptr_reg);

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
            return;
        }

        // -------- Step 3: no ctor — initialize fields directly. --------
        //
        // For a StructureType with scalar fields, we emit one
        // Store* per field. Reference-typed fields (inline structs,
        // arrays, pointers) are not handled yet and are skipped with
        // a warning. This is the minimum needed to make
        //     (new 'heap 'vector :x 5 :y 6 :z 7 :w 8)
        // produce a vector whose fields are set.
        auto *st = dynamic_cast<StructureType *>(type);
        if (st) { emit_field_init_stores(fn, ptr_reg, st); }

        fn.set_temp_reg(this, ptr_reg);
    }

    void NewNode::emit_field_init_stores(FunctionNode &fn, u8 base_ptr_reg, StructureType *st) {
        // Build a map name -> Field from st.
        // For keyword-args: find the field by name.
        // For positional: take fields in declaration order.
        //
        // For each field with an init:
        //   1. Emit the init expression -> value_reg.
        //   2. Compute address: r_addr = base_ptr_reg + offset (IAddImm).
        //   3. Emit Store* by field type.
        //
        // Scalar fields only for now. Reference fields (inline, array,
        // pointer) are skipped with a warning.

        TypeSystem &ts = TypeSystem::instance();

        auto emit_one = [&](const Field &field, ExpressionNode *init) {
            if (!init) return;

            Type *field_type = ts.lookup_type_no_throw(field.type().base_type());
            if (!field_type) {
                lg::warn("NewNode: field '{}' has unknown type '{}', skipped", field.name(),
                         field.type().print());
                return;
            }

            if (field_type->is_reference()) {
                lg::warn("NewNode: field '{}' has reference type '{}'; "
                         "inline/pointer initialization not yet supported, skipped",
                         field.name(), field.type().print());
                return;
            }

            // 1. Compute address.
            u8 addr_reg = fn.alloc_temp_reg(nullptr);
            fn.add_instruction_imm_reg(Opcode::IAddImm, addr_reg, base_ptr_reg,
                                       static_cast<u8>(field.offset() & 0xFF));

            // 2. Evaluate init into a temp.
            init->emit(fn);
            u8 value_reg = fn.get_temp_reg(init);

            // 3. Store. The Store* opcodes use: a = address, b = value.
            Opcode store_op = store_op_for(field_type);
            fn.add_instruction(store_op, addr_reg, value_reg, 0);
        };

        // Keyword-args.
        if (!m_fields.empty()) {
            for (const auto &fi : m_fields) {
                // Find field by name.
                const Field *found = nullptr;
                for (const auto &f : st->fields()) {
                    if (f.name() == fi.name) {
                        found = &f;
                        break;
                    }
                }
                if (!found) {
                    lg::warn("NewNode: type '{}' has no field '{}', skipped", st->get_name(),
                             fi.name);
                    continue;
                }
                emit_one(*found, fi.value.get());
            }
            return;
        }

        // Positional-args.
        const auto &fields = st->fields();
        for (size_t i = 0; i < m_args.size() && i < fields.size(); ++i) {
            emit_one(fields[i], m_args[i].get());
        }
        if (m_args.size() > fields.size()) {
            lg::warn("NewNode: type '{}' has {} fields but {} positional args, "
                     "extra ignored",
                     st->get_name(), fields.size(), m_args.size());
        }
    }

    std::string NewNode::to_string() const {
        std::string result = "(new " + m_allocation + " ";
        if (m_is_inline_array) {
            result += "'inline-array " + m_elem_type_name + " " + std::to_string(m_array_size);
            for (const auto &e : m_elements) { result += " " + (e ? e->to_string() : "none"); }
            return result + ")";
        }
        result += m_type.print();
        for (const auto &f : m_fields) {
            result += " :" + f.name + " " + (f.value ? f.value->to_string() : "none");
        }
        for (const auto &a : m_args) { result += " " + (a ? a->to_string() : "none"); }
        return result + ")";
    }

    ProgramBinaryElement NewNode::generate(DataSegment &state) {
        // Static initialization is handled by DataDeclarationNode, not
        // by NewNode directly. A bare NewNode never produces a binary.
        // Returning an empty element keeps the interface satisfied;
        // the caller (DataDeclarationNode::generate) is the one that
        // actually builds the payload.
        (void)state;
        return ProgramBinaryElement(0);
    }
} // namespace sootc