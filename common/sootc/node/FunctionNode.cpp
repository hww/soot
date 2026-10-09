// FunctionNode.cpp
#include "FunctionNode.hpp"
#include "ExpressionNode.hpp"

#include "common/carbon/vm/Instructions.hpp"
#include "common/carbon/file/DCScript.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/StringIdManager.hpp"
#include "common/util/Log.hpp"
#include "common/sootc/libs/CompareOp.hpp"
#include "common/sootc/node/Node.hpp"
#include "common/sootc/libs/StringsTable.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace sootc {

    // ========================================================================
    // Constructor
    // ========================================================================
    FunctionNode::FunctionNode(const std::string &name)
        : Node(NodeType::FunctionNode), m_name(name) {}

    // ========================================================================
    // to_string
    // ========================================================================
    std::string FunctionNode::to_string() const {
        return "FunctionNode(name=" + m_name + ", params=" + std::to_string(m_param_count) +
               ", locals=" + std::to_string(m_variables.size() - m_param_count) + ")";
    }

    // ========================================================================
    // Parameters
    // ========================================================================
    void FunctionNode::add_parameter(const std::string &name, Type *type) {
        u8 reg = static_cast<u8>(m_variables.size());
        m_variables.emplace_back(name, type, reg, true);
        m_variable_index[name] = m_variables.size() - 1;
        m_param_count++;
        m_param_map[name] = reg;
    }

    void FunctionNode::insert_parameter_at(size_t index, const std::string &name, Type *type) {
        if (index > m_variables.size()) { index = m_variables.size(); }

        m_variables.insert(m_variables.begin() + index,
                           VariableInfo(name, type, static_cast<u8>(index), /*is_param=*/true));

        // Renumber every entry so that register indices match their positions.
        for (size_t i = 0; i < m_variables.size(); ++i) {
            m_variables[i] = VariableInfo(m_variables[i].name(), m_variables[i].type(),
                                          static_cast<u8>(i), m_variables[i].is_parameter());
        }

        m_variable_index.clear();
        for (size_t i = 0; i < m_variables.size(); ++i) {
            m_variable_index[m_variables[i].name()] = i;
        }

        ++m_param_count;
    }
    // ========================================================================
    // Local variables
    // ========================================================================
    void FunctionNode::add_local_variable(const std::string &name, Type *type) {
        u8 reg = static_cast<u8>(m_variables.size());
        m_variables.emplace_back(name, type, reg, false);
        m_variable_index[name] = m_variables.size() - 1;
    }

    const VariableInfo *FunctionNode::lookup_variable(const std::string &name) const {
        auto it = m_variable_index.find(name);
        if (it != m_variable_index.end()) { return &m_variables[it->second]; }
        return nullptr;
    }

    u8 FunctionNode::get_variable_reg(const std::string &name) const {
        auto *info = lookup_variable(name);
        return info ? info->reg() : 0;
    }

    Type *FunctionNode::get_variable_type(const std::string &name) const {
        auto *info = lookup_variable(name);
        return info ? info->type() : nullptr;
    }

    // ========================================================================
    // Temp registers
    // ========================================================================
    u8 FunctionNode::alloc_temp_reg(Type *type) {
        (void)type;
        u8 base = static_cast<u8>(m_variables.size());
        return static_cast<u8>(base + m_next_temp_reg++);
    }

    void FunctionNode::set_temp_reg(const Node *node, u8 reg) { m_temp_regs[node] = reg; }

    u8 FunctionNode::get_temp_reg(const Node *node) const {
        auto it = m_temp_regs.find(node);
        if (it == m_temp_regs.end()) {
            throw std::runtime_error(fmt::format("get_temp_reg: node '{}' ({}) has no temp reg",
                                                 node ? node->to_string() : "nullptr",
                                                 node ? node->node_type() : "nullptr"));
        }
        return it->second;
    }

    // ========================================================================
    // General register access
    // ========================================================================
    u8 FunctionNode::get_reg(const Node *node) const {
        auto tit = m_temp_regs.find(node);
        if (tit != m_temp_regs.end()) return tit->second;

        if (auto *var_info = lookup_variable(node->to_string())) { return var_info->reg(); }

        return 0;
    }

    // ========================================================================
    // Constants
    // ========================================================================
    u16 FunctionNode::add_constant(u64 value, ConstKind kind) {
        auto it = std::find(m_constants.begin(), m_constants.end(), value);
        if (it != m_constants.end()) { return static_cast<u16>(it - m_constants.begin()); }
        u16 idx = static_cast<u16>(m_constants.size());
        m_constants.push_back(value);
        m_constants_kind.push_back(static_cast<u8>(kind));
        return idx;
    }

    // ========================================================================
    // Instructions
    // ========================================================================
    void FunctionNode::add_instruction(Opcode op, u8 dest, u8 src1, u8 src2) {
        m_instructions.emplace_back(InstructionFactory::abc(op, dest, src1, src2));
    }

    void FunctionNode::add_instruction_imm_u16(Opcode op, u8 dest, u16 imm) {
        Instruction instr;
        instr.opcode = op;
        instr.destination = dest;
        instr.set_lo_hi(imm);
        m_instructions.push_back(instr);
    }

    void FunctionNode::add_instruction_imm_reg(Opcode op, u8 dest, u8 base, u8 imm) {
        Instruction instr;
        instr.opcode = op;
        instr.destination = dest;
        instr.operand1 = base;
        instr.operand2 = imm;
        m_instructions.push_back(instr);
    }

    // ========================================================================
    // Labels
    // ========================================================================
    void FunctionNode::add_label(const std::string &name) {
        m_labels[name] = static_cast<u32>(m_instructions.size());
    }

    std::string FunctionNode::create_unique_label(const std::string &prefix) {
        return prefix + "_" + std::to_string(m_label_counter++);
    }

    void FunctionNode::add_branch_reference(const std::string &label) {
        if (m_instructions.empty()) {
            throw std::runtime_error("add_branch_reference: no instruction to patch");
        }
        u32 pos = static_cast<u32>(m_instructions.size()) - 1;
        m_unresolved_branches.emplace_back(label, pos);
    }

    void FunctionNode::resolve_branches() {
        for (auto &[label, pos] : m_unresolved_branches) {
            auto it = m_labels.find(label);
            if (it == m_labels.end()) { continue; }
            u32 target = it->second;
            m_instructions[pos].set_lo_hi(static_cast<u16>(target));
        }
        m_unresolved_branches.clear();
    }

    void FunctionNode::add_compare(u8 left, u8 right, CompareOp op) {
        switch (op) {
        case CompareOp::EQ: add_instruction(Opcode::IEqual, left, left, right); break;
        case CompareOp::NE: add_instruction(Opcode::INotEqual, left, left, right); break;
        case CompareOp::LT: add_instruction(Opcode::ILessThan, left, left, right); break;
        case CompareOp::LE: add_instruction(Opcode::ILessThanEqual, left, left, right); break;
        case CompareOp::GT: add_instruction(Opcode::IGreaterThan, left, left, right); break;
        case CompareOp::GE: add_instruction(Opcode::IGreaterThanEqual, left, left, right); break;
        }
    }

    // ========================================================================
    // Code generation
    // ========================================================================
    void FunctionNode::set_body(std::unique_ptr<ExpressionNode> body) { m_body = std::move(body); }

    void FunctionNode::emit_body(ExpressionNode *body) {
        if (!body) {
            lg::warn("emit_body: null body for function '{}'", m_name);
            return;
        }
        lg::info("emit_body: function '{}', body type = {}", m_name, body->node_type());

        m_instructions.clear();
        m_constants.clear();
        m_temp_regs.clear();
        m_next_temp_reg = 0;

        // Prologue: copy arguments from r24+ into local registers.
        for (size_t i = 0; i < m_param_count; ++i) {
            const VariableInfo &info = m_variables[i];
            u8                  local_reg = info.reg();
            u8                  arg_reg = static_cast<u8>(ARG_REGISTERS_OFFSET + i);
            add_instruction(Opcode::Move, local_reg, arg_reg, 0);
        }

        // Body.
        body->emit(*this);

        // If last instruction is not Return — add Return.
        if (m_instructions.empty() || m_instructions.back().opcode != Opcode::Return) {
            u8   result_reg = 0;
            auto it = m_temp_regs.find(body);
            if (it != m_temp_regs.end()) { result_reg = it->second; }
            add_instruction(Opcode::Return, result_reg, 0, 0);
        }

        resolve_branches();
    }

    void FunctionNode::emit_body() {
        if (!m_body) return;
        emit_body(m_body.get());
    }

    // ========================================================================
    // Serialization
    // ========================================================================
    /// @brief Serialise this function into a ProgramBinaryElement.
    /// @details Layout:
    ///            - ScriptLambda header (fixed fields + pointers to symbols
    ///              and constants),
    ///            - instruction stream (one Instruction per element),
    ///            - constant pool (one u64 per element).
    ///
    ///          Relocation policy:
    ///            - ScriptLambda: the two pointer fields (m_pSymbols, m_pConstants)
    ///              must be relocated. They live at 8-byte slots 1 and 2.
    ///            - Instructions and constants: no relocations.
    ProgramBinaryElement FunctionNode::build_binary(const std::string &module_name,
                                                    StringsTable       &strings_table) {
        (void)module_name;
        (void)strings_table;

        // Compute the offsets of the constant pool with the same alignment
        // rule that build_binary uses when it writes them: the payload is
        // padded to an 8-byte boundary before the first constant, so
        // m_pSymbols must point at that aligned offset, not at the raw
        // end-of-instructions offset.
        const u64 instr_end = sizeof(ScriptLambda) + m_instructions.size() * sizeof(Instruction);
        const u64 constants_start = (instr_end + 7) & ~u64{7};

        u64 total_size = sizeof(sid64) + sizeof(ScriptLambda) +
                         m_instructions.size() * sizeof(Instruction) +
                         (constants_start - instr_end) + m_constants.size() * sizeof(u64);

        ProgramBinaryElement element(total_size);

        element.m_entry = {.m_nameID = StringId(m_name).value,
                           .m_typeId = StringId("script-lambda").value,
                           .m_entryPtr = nullptr};

        lg::info("FunctionNode::build_binary for entry {}", element.m_entry.to_string());

        ScriptLambda lambda = {StringId("script-lambda").value,
                               reinterpret_cast<u64 *>(sizeof(ScriptLambda)),
                               reinterpret_cast<u64 *>(constants_start),
                               StringId("function").value,
                               (sizeof(ScriptLambda) + sizeof(Instruction) * m_instructions.size() +
                                sizeof(lambda_symbol_entry) * m_constants.size()),
                               0x0,
                               DEADBEEF,
                               0x0,
                               static_cast<u32>(m_instructions.size()),
                               static_cast<u32>(m_constants.size()),
                               -1,
                               StringId("global").value,
                               0x0};

        // Diagnostic: print the exact size so we can craft the relocation mask.
        lg::info("sizeof(ScriptLambda) = {}", sizeof(ScriptLambda));

        // TODO: verify ScriptLambda layout. For now, mark only slots 1 and 2
        // as relocatable (the two pointer fields m_pSymbols and m_pConstants).
        // If sizeof(ScriptLambda) is not 88 bytes, adjust the list length.
        element.push_value_with_ptr(lambda, PTR_FIELD(ScriptLambda, m_pInstruction),
                                    PTR_FIELD(ScriptLambda, m_pSymbols));

                for (const Instruction &instr : m_instructions) { element.push_value(instr); }

         for (size_t i = 0; i < m_constants.size(); ++i) {
            lg::info("build_binary: i={}, value=0x{:016X}, kind={}", i, m_constants[i],
                     (i < m_constants_kind.size() ? (u32)m_constants_kind[i] : 999u));

            // Constants are 8-byte values and every relocation bit covers a
            // whole 8-byte slot. Pad the payload to an 8-byte boundary so a
            // constant never straddles a slot boundary, and its relocation
            // bit lands on the slot that actually contains it.
            while (element.m_rawData.size() % 8 != 0) {
                element.m_rawData.push_back(std::byte{0});
            }
            {
                const size_t needed = (element.m_rawData.size() + 7) / 8;
                while (element.m_relocTable.size() < needed) {
                    element.m_relocTable.push_back(false);
                }
            }

            const u64 offset_in_payload = element.m_rawData.size();
            element.push_value(m_constants[i]);
            lg::info("AFTER push_value: i={}, rawData.size()={}", i, element.m_rawData.size());

            // Only string constants require relocation against the global
            // string table. Their value is currently the index of the
            // string in StringsTable::offsets(); it will be replaced by
            // FileNode::make_binary with the absolute file offset of the
            // string, and the slot will be marked as relocatable.
            //
            // IMPORTANT: we do NOT duplicate the string inside the
            // function payload. The string lives in the global string
            // table at the end of the file, and the slot holds a pointer
            // to it (as a file-relative offset that the loader turns into
            // an absolute pointer).
            const bool is_string = (i < m_constants_kind.size() &&
                                    m_constants_kind[i] == static_cast<u8>(ConstKind::STRING));

            if (is_string) {
                // m_constants[i] holds the byte offset of the string inside the
                // StringsTable (as returned by lookup_or_add), NOT an index.
                const u32 str_offset_in_table = static_cast<u32>(m_constants[i]);
                element.m_stringConstantSlots.push_back({offset_in_payload, str_offset_in_table});

                // Mark the slot as relocatable so read_reloc_table() converts the
                // file-relative string offset stored here into an absolute pointer.
                // FileNode::make_binary will temporarily clear this bit for
                // adjust_offsets() and restore it before folding into the final
                // bitmap.
                const size_t slot_bit = offset_in_payload / 8;
                if (slot_bit < element.m_relocTable.size()) {
                    element.m_relocTable[slot_bit] = true;
                }
            }
        }

        return element;
    }

} // namespace sootc