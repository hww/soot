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

        // 1. Resolve the constructor "<type>-new".
        const std::string ctor_name = type_name + "-new";

        u8  ctor_reg = fn.alloc_temp_reg(nullptr);
        u16 ctor_st = fn.add_constant(StringId(ctor_name).value, FunctionNode::ConstKind::SID);
        fn.add_instruction_imm_u16(Opcode::LookupPointer, ctor_reg, ctor_st);

        // 2. Build the argument list: allocation SID, type-to-make SID,
        //    then user arguments.
        std::vector<u8> arg_regs;

        // 2a. allocation SID (e.g. "global").
        {
            u8  reg = fn.alloc_temp_reg(nullptr);
            u16 st = fn.add_constant(StringId(m_allocation).value, FunctionNode::ConstKind::SID);
            fn.add_instruction_imm_u16(Opcode::LookupInt, reg, st);
            arg_regs.push_back(reg);
        }

        // 2b. type-to-make SID (e.g. "vec3").
        {
            u8  reg = fn.alloc_temp_reg(nullptr);
            u16 st = fn.add_constant(StringId(type_name).value, FunctionNode::ConstKind::SID);
            fn.add_instruction_imm_u16(Opcode::LookupInt, reg, st);
            arg_regs.push_back(reg);
        }

        // 2c. User arguments.
        //
        // For 'static' we don't get here (throws above).
        // For 'global'/'heap'/'stack' the current implementation accepts
        // BOTH `:field value` pairs (via m_fields) AND positional args
        // (via m_args). We emit them in the order the structure declares
        // its fields, then append positional args.
        {
            auto *structure = TypeSystem::instance().lookup_type_no_throw(m_type.base_type());
            auto *st = dynamic_cast<StructureType *>(structure);

            // Collect all fields, annotate with their offset in the struct.
            struct EmitItem {
                const FieldInit *field;
                int              offset;
            };
            std::vector<EmitItem> sorted;
            sorted.reserve(m_fields.size());

            for (const auto &f : m_fields) {
                int off = 0;
                if (st) {
                    Field lookup;
                    if (st->lookup_field(f.name, &lookup)) {
                        off = lookup.offset();
                    } else {
                        throw std::runtime_error(
                            fmt::format("NewNode::emit: type '{}' has no field '{}'",
                                        m_type.base_type(), f.name));
                    }
                }
                sorted.push_back({&f, off});
            }

            std::sort(sorted.begin(), sorted.end(),
                      [](const EmitItem &a, const EmitItem &b) { return a.offset < b.offset; });

            // Emit field values in struct order.
            for (const auto &item : sorted) {
                item.field->value->emit(fn);
                arg_regs.push_back(fn.get_temp_reg(item.field->value.get()));
            }

            // Then any positional args.
            for (auto &a : m_args) {
                a->emit(fn);
                arg_regs.push_back(fn.get_temp_reg(a.get()));
            }
        }

        // 3. Move into r24+.
        for (size_t i = 0; i < arg_regs.size(); ++i) {
            fn.add_instruction(Opcode::Move, static_cast<u8>(ARG_REGISTERS_OFFSET + i), arg_regs[i],
                               0);
        }

        // 4. Call the constructor and store the result.
        //
        // `<type>-new` is a NATIVE function (registered in Globals or in
        // NativeFunctionRegistry). ScriptLambdas are not used for runtime
        // allocation, so we must emit CallFf (native call), not Call.
        u8 ret_reg = fn.alloc_temp_reg(nullptr);
        // `point-new` is always a native constructor.
        fn.add_instruction(Opcode::CallFf, ret_reg, ctor_reg, static_cast<u8>(arg_regs.size()));
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