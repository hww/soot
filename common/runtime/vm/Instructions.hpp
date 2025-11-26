#pragma once

#include "common/runtime/ForwardDeclarations.hpp"
#include "common/runtime/lib/Types.hpp"
#include "common/runtime/lib/StringId.hpp"
#include <cstdint>
#include <format>
#include <unordered_map>

using namespace runtime::lib;

namespace runtime::vm {

    // ============================================================================
    // Instruction Opcodes
    // ============================================================================

    enum class Opcode : u8 {
        // ============================================================
        // Control Flow Instructions
        // ============================================================
        RETURN = 0x00,
        MOVE = 0x01,
        CALL = 0x02,
        CALL_NATIVE = 0x03,
        BRANCH = 0x04,
        BRANCH_IF = 0x05,
        BRANCH_IF_NOT = 0x06,

        // ============================================================
        // Integer Arithmetic Operations
        // ============================================================
        ADD_INT = 0x10,
        SUB_INT = 0x11,
        MUL_INT = 0x12,
        DIV_INT = 0x13,
        MOD_INT = 0x14,
        ABS_INT = 0x15,
        NEG_INT = 0x16,
        ASH_INT = 0x17,
        TO_INT = 0x18,

        // ============================================================
        // Integer Immediate Operations
        // ============================================================
        LOAD_IMMEDIATE_INT = 0x20,
        ADD_IMM = 0x21,
        SUB_IMM = 0x22,
        MUL_IMM = 0x23,
        DIV_IMM = 0x24,

        // ============================================================
        // Floating Point Arithmetic Operations
        // ============================================================
        ADD_FLOAT = 0x30,
        SUB_FLOAT = 0x31,
        MUL_FLOAT = 0x32,
        DIV_FLOAT = 0x33,
        MOD_FLOAT = 0x34,
        ABS_FLOAT = 0x35,
        NEG_FLOAT = 0x36,
        TO_FLOAT = 0x37,

        // ============================================================
        // Comparison Operations
        // ============================================================
        CMP_EQUAL = 0x40,
        CMP_NOT_EQUAL = 0x41,
        CMP_GT = 0x42,
        CMP_GT_EQUAL = 0x43,
        CMP_LT = 0x44,
        CMP_LT_EQUAL = 0x45,
        CMP_FLOAT_EQUAL = 0x46,
        CMP_FLOAT_NOT_EQUAL = 0x47,
        CMP_FLOAT_GT = 0x48,
        CMP_FLOAT_GT_EQUAL = 0x49,
        CMP_FLOAT_LT = 0x4A,
        CMP_FLOAT_LT_EQUAL = 0x4B,

        // ============================================================
        // Logical Operations
        // ============================================================
        LOG_AND = 0x50,
        LOG_OR = 0x51,
        LOG_NOT = 0x52,

        // ============================================================
        // Bitwise Operations
        // ============================================================
        BIT_AND = 0x60,
        BIT_OR = 0x61,
        BIT_XOR = 0x62,
        BIT_NOR = 0x63,
        BIT_NOT = 0x64,

        // ============================================================
        // Utility Operations
        // ============================================================
        LOAD_ARGC = 0x70,
        GET_SID_STRING = 0x71,

        // ============================================================
        // Lookup Operations (Environment Access)
        // ============================================================
        LOOKUP_INT = 0x80,
        LOOKUP_FLOAT = 0x81,
        LOOKUP_POINTER = 0x82,

        // ============================================================
        // Indirect Load Operations (Memory Access via Pointer)
        // ============================================================
        LOAD_IND_INT = 0x90,
        LOAD_IND_FLOAT = 0x91,
        LOAD_IND_POINTER = 0x92,

        // ============================================================
        // Indirect Store Operations (Memory Access via Pointer)
        // ============================================================
        STORE_IND_INT = 0xA0,
        STORE_IND_FLOAT = 0xA1,
        STORE_IND_POINTER = 0xA2,

        // ============================================================
        // Static Load Operations (Data Segment Access)
        // ============================================================
        LOAD_STATIC_INT = 0xB0,
        LOAD_STATIC_FLOAT = 0xB1,
        LOAD_STATIC_POINTER = 0xB2,

        // ============================================================
        // Immediate Value Load Operations
        // ============================================================
        LOAD_IMMEDIATE_FLOAT = 0xC0,
        LOAD_IMMEDIATE_PTR = 0xC1,

        // ============================================================
        // Extended Operations (Reserved for Future Use)
        // ============================================================
        CALL_BY_NAME = 0xD0,  // For direct function calling by name
        CREATE_LAMBDA = 0xD1, // For creating lambda closures

        // ============================================================
        // Debug Operations
        // ============================================================
        DEBUG_BREAK = 0xFE,   // For debugger breakpoints
        NOOP = 0xFF           // No operation
    };

    // ============================================================================
    // Instruction Structure
    // ============================================================================

#pragma pack(push, 1)
    struct Instruction {
        union {
            u32 as_u32;

            struct {
                Opcode opcode : 8;
                u8 a;          // Destination register / condition register
                u8 b;          // Source register 1
                u8 c;          // Source register 2
            };

            struct {
                u8 : 8;        // padding for opcode
                u8 a_imm;      // Destination register for immediate
                i16 imm16;     // 16-bit immediate value
            };

            struct {
                u8 : 8;        // padding for opcode  
                u8 a_k;        // Destination register
                u16 k;         // 16-bit unsigned immediate
            };
        };

        
        // ------------------------------------------------------------------------
        // Constructors (исправлены для устранения неоднозначности)
        // ------------------------------------------------------------------------

        Instruction() : as_u32(0) {}

        // For 3-register operations: op a, b, c
        static Instruction create_abc(Opcode op, u8 a, u8 b, u8 c) {
            Instruction instr;
            instr.opcode = op;
            instr.a = a;
            instr.b = b;
            instr.c = c;
            return instr;
        }

        // For 2-register operations: op a, b  
        static Instruction create_ab(Opcode op, u8 a, u8 b) {
            Instruction instr;
            instr.opcode = op;
            instr.a = a;
            instr.b = b;
            instr.c = 0;
            return instr;
        }

        // For 1-register operations: op a
        static Instruction create_a(Opcode op, u8 a) {
            Instruction instr;
            instr.opcode = op;
            instr.a = a;
            instr.b = 0;
            instr.c = 0;
            return instr;
        }

        // For signed immediate operations: op a, imm16
        static Instruction create_imm(Opcode op, u8 a, i16 imm) {
            Instruction instr;
            instr.a_imm = a;
            instr.imm16 = imm;
            instr.opcode = op;
            return instr;
        }

        // For unsigned immediate: op a, k
        static Instruction create_k(Opcode op, u8 a, u16 k) {
            Instruction instr;
            instr.a_k = a;
            instr.k = k;
            instr.opcode = op;
            return instr;
        }


        // ------------------------------------------------------------------------
        // Utility Methods
        // ------------------------------------------------------------------------

        bool has_immediate() const {
            switch (opcode) {
            case Opcode::LOAD_IMMEDIATE_INT:
            case Opcode::ADD_IMM:
            case Opcode::SUB_IMM:
            case Opcode::MUL_IMM:
            case Opcode::DIV_IMM:
            case Opcode::BRANCH:
            case Opcode::BRANCH_IF:
            case Opcode::BRANCH_IF_NOT:
            case Opcode::LOAD_STATIC_INT:
            case Opcode::LOAD_STATIC_FLOAT:
            case Opcode::LOAD_STATIC_POINTER:
            case Opcode::LOOKUP_INT:
            case Opcode::LOOKUP_FLOAT:
            case Opcode::LOOKUP_POINTER:
                return true;
            default:
                return false;
            }
        }

        std::string to_string() const {
            if (has_immediate()) {
                return std::format("({:02x} {:2d} {:4d})",
                    static_cast<u32>(opcode), a, imm16);
            }
            else {
                return std::format("({:02x} {:2d} {:2d} {:2d})",
                    static_cast<u32>(opcode), a, b, c);
            }
        }
    };
#pragma pack(pop)

    static_assert(sizeof(Instruction) == 4, "Instruction must be 4 bytes");

    // ============================================================================
    // Instruction Metadata
    // ============================================================================

    struct InstructionInfo {
        Opcode opcode;
        StringId name;
        u8 operand_count;
        bool has_immediate;

        std::string to_string() const {
            return std::format("{} (op:{:02x}, operands:{}, imm:{})",
                lib::to_string(name),
                static_cast<u32>(opcode),
                operand_count,
                has_immediate);
        }
    };

    // ============================================================================
    // Instruction Table
    // ============================================================================

    class InstructionTable {
    public:
        static InstructionTable& instance() {
            static InstructionTable table;
            return table;
        }

        const InstructionInfo* get_info(Opcode opcode) const {
            auto it = opcode_to_info_.find(opcode);
            return it != opcode_to_info_.end() ? &it->second : nullptr;
        }

        const InstructionInfo* get_info_by_name(StringId name) const {
            auto it = name_to_info_.find(name);
            return it != name_to_info_.end() ? &it->second : nullptr;
        }

        StringId get_opcode_name(Opcode opcode) const {
            auto info = get_info(opcode);
            return info ? info->name : SID("unknown");
        }

    private:
        InstructionTable() {
            initialize_table();
        }

        void initialize_table() {
            // Control flow
            add_instruction(Opcode::RETURN,         "ret", 1, false);
            add_instruction(Opcode::MOVE,           "move", 2, false);
            add_instruction(Opcode::CALL,           "call", 3, false);
            add_instruction(Opcode::CALL_NATIVE,    "calln", 3, false);
            add_instruction(Opcode::BRANCH,         "br", 1, true);
            add_instruction(Opcode::BRANCH_IF,      "brif", 1, true);
            add_instruction(Opcode::BRANCH_IF_NOT,  "brno", 1, true);

            // Integer operations
            add_instruction(Opcode::ADD_INT, "add", 3, false);
            add_instruction(Opcode::SUB_INT, "sub", 3, false);
            add_instruction(Opcode::MUL_INT, "mul", 3, false);
            add_instruction(Opcode::DIV_INT, "div", 3, false);
            add_instruction(Opcode::MOD_INT, "mod", 3, false);
            add_instruction(Opcode::ABS_INT, "abs", 2, false);
            add_instruction(Opcode::NEG_INT, "neg", 2, false);
            add_instruction(Opcode::ASH_INT, "ash", 2, false);
            add_instruction(Opcode::TO_INT,  "toi", 2, false);

            // Add more instructions as needed...
        }

        //void add_instruction(Opcode opcode, StringId name, u8 operand_count, bool has_immediate) {
        //    InstructionInfo info{ opcode, name, operand_count, has_immediate };
        //    opcode_to_info_.emplace(opcode, info);
        //    name_to_info_.emplace(name, info);
        //}

        void add_instruction(Opcode opcode, const char* name, u8 operand_count, bool has_immediate) {
            auto name_id = string_id::register_string(name);
            InstructionInfo info{ opcode, name_id, operand_count, has_immediate};
            opcode_to_info_.emplace(opcode, info);
            name_to_info_.emplace(name_id, info);
        }

        std::unordered_map<Opcode, InstructionInfo> opcode_to_info_;
        std::unordered_map<StringId, InstructionInfo> name_to_info_;
    };

    // ============================================================================
    // Utility Functions
    // ============================================================================

    inline std::string opcode_to_string(Opcode opcode) {
        return lib::to_string(InstructionTable::instance().get_opcode_name(opcode));
    }

    inline std::ostream& operator<<(std::ostream& os, const Instruction& instr) {
        return os << instr.to_string();
    }

} // namespace vm