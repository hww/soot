#pragma once

#include "CommonTypes.hpp"
#include "Opcodes.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <ostream>
#include <string>

namespace carbon {

    // Register file layout:
    //   r0..r23   local variables (24 registers)
    //   r24..r33  arguments       (10 registers)
    constexpr u32 MAX_REGISTERS = 34;
    constexpr u32 ARG_REGISTERS_OFFSET = 24;
    constexpr u32 LOCAL_REGISTERS_OFFSET = 0;
    constexpr u32 MAX_LOCALS = ARG_REGISTERS_OFFSET - LOCAL_REGISTERS_OFFSET; // 24
    constexpr u32 MAX_ARGS = MAX_REGISTERS - ARG_REGISTERS_OFFSET;            // 10

    /// @brief Type of an instruction operand.
    enum class OperandType : u8 {
        NONE,    ///< operand unused
        REG,     ///< register index
        IMM_U8,  ///< 8-bit unsigned immediate
        IMM_I16, ///< 16-bit signed immediate
        IMM_U16, ///< 16-bit unsigned immediate (symbol-table index, argument count)
    };

    /// @brief Static type of a symbol-table entry, when known.
    enum class StaticType {
        NONE,    ///< unknown
        POINTER, ///< pointer
        I8,      ///< int8
        U8,      ///< uint8
        I16,     ///< int16
        U16,     ///< uint16
        I32,     ///< int32
        U32,     ///< uint32
        I64,     ///< int64
        U64,     ///< uint64
        FLOAT,   ///< f32
        DOUBLE,  ///< f64
        SID      ///< SID64
    };

    /// @brief Metadata for a single Opcode: name, operand kinds, and static type.
    struct InstructionInfo {
        Opcode      opcode;      ///< opcode value
        const char *name;        ///< human-readable mnemonic
        OperandType a_type;      ///< destination operand kind
        OperandType b_type;      ///< first source operand kind
        OperandType c_type;      ///< second source operand kind
        StaticType  static_type; ///< static type of the symbol-table value, if any

        [[nodiscard]] size_t operands_count() const noexcept {
            size_t cnt = 0;
            if (a_type != OperandType::NONE) ++cnt;
            if (b_type != OperandType::NONE) ++cnt;
            if (c_type != OperandType::NONE) ++cnt;
            return cnt;
        }

        [[nodiscard]] bool is_reg(OperandType t) const noexcept { return t == OperandType::REG; }
        [[nodiscard]] bool is_imm(OperandType t) const noexcept {
            return t == OperandType::IMM_U8 || t == OperandType::IMM_I16 ||
                   t == OperandType::IMM_U16;
        }
        [[nodiscard]] bool is_used(OperandType t) const noexcept { return t != OperandType::NONE; }
    };

    /// @brief Look up instruction metadata by opcode.
    [[nodiscard]] inline const InstructionInfo *get_instruction_info(Opcode op);

    /// @brief Convert an opcode to its human-readable mnemonic.
    [[nodiscard]] inline const char *opcode_mnemonic(Opcode opcode) noexcept {
        switch (opcode) {
        case Opcode::Return: return "Return";
        case Opcode::IAdd: return "IAdd";
        case Opcode::ISub: return "ISub";
        case Opcode::IMul: return "IMul";
        case Opcode::IDiv: return "IDiv";
        case Opcode::FAdd: return "FAdd";
        case Opcode::FSub: return "FSub";
        case Opcode::FMul: return "FMul";
        case Opcode::FDiv: return "FDiv";
        case Opcode::LoadStaticInt: return "LoadStaticInt";
        case Opcode::LoadStaticFloat: return "LoadStaticFloat";
        case Opcode::LoadStaticPointer: return "LoadStaticPointer";
        case Opcode::LoadU16Imm: return "LoadU16Imm";
        case Opcode::LoadInt: return "LoadU32";
        case Opcode::LoadFloat: return "LoadFloat";
        case Opcode::LoadPointer: return "LoadPointer";
        case Opcode::StoreInt: return "StoreInt";
        case Opcode::StoreFloat: return "StoreFloat";
        case Opcode::StorePointer: return "StorePointer";
        case Opcode::LookupInt: return "LookupInt";
        case Opcode::LookupFloat: return "LookupFloat";
        case Opcode::LookupPointer: return "LookupPointer";
        case Opcode::MoveInt: return "MoveInt";
        case Opcode::MoveFloat: return "MoveFloat";
        case Opcode::MovePointer: return "MovePointer";
        case Opcode::CastInteger: return "CastInteger";
        case Opcode::CastFloat: return "CastFloat";
        case Opcode::Call: return "Call";
        case Opcode::CallFf: return "CallFf";
        case Opcode::IEqual: return "IEqual";
        case Opcode::IGreaterThan: return "IGreaterThan";
        case Opcode::IGreaterThanEqual: return "IGreaterThanEqual";
        case Opcode::ILessThan: return "ILessThan";
        case Opcode::ILessThanEqual: return "ILessThanEqual";
        case Opcode::FEqual: return "FEqual";
        case Opcode::FGreaterThan: return "FGreaterThan";
        case Opcode::FGreaterThanEqual: return "FGreaterThanEqual";
        case Opcode::FLessThan: return "FLessThan";
        case Opcode::FLessThanEqual: return "FLessThanEqual";
        case Opcode::IMod: return "IMod";
        case Opcode::FMod: return "FMod";
        case Opcode::IAbs: return "IAbs";
        case Opcode::FAbs: return "FAbs";
        case Opcode::GoTo: return "GoTo";
        case Opcode::Label: return "Label";
        case Opcode::Branch: return "Branch";
        case Opcode::BranchIf: return "BranchIf";
        case Opcode::BranchIfNot: return "BranchIfNot";
        case Opcode::OpLogNot: return "OpLogNot";
        case Opcode::OpBitAnd: return "OpBitAnd";
        case Opcode::OpBitNot: return "OpBitNot";
        case Opcode::OpBitOr: return "OpBitOr";
        case Opcode::OpBitXor: return "OpBitXor";
        case Opcode::OpBitNor: return "OpBitNor";
        case Opcode::OpLogAnd: return "OpLogAnd";
        case Opcode::OpLogOr: return "OpLogOr";
        case Opcode::INeg: return "INeg";
        case Opcode::FNeg: return "FNeg";
        case Opcode::LoadParamCnt: return "LoadParamCnt";
        case Opcode::IAddImm: return "IAddImm";
        case Opcode::ISubImm: return "ISubImm";
        case Opcode::IMulImm: return "IMulImm";
        case Opcode::IDivImm: return "IDivImm";
        case Opcode::LoadStaticI32Imm: return "LoadStaticI32Imm";
        case Opcode::LoadStaticFloatImm: return "LoadStaticFloatImm";
        case Opcode::LoadStaticPointerImm: return "LoadStaticPointerImm";
        case Opcode::IntAsh: return "IntAsh";
        case Opcode::Move: return "Move";
        case Opcode::LoadStaticU32Imm: return "LoadStaticU32Imm";
        case Opcode::LoadStaticI8Imm: return "LoadStaticI8Imm";
        case Opcode::LoadStaticU8Imm: return "LoadStaticU8Imm";
        case Opcode::LoadStaticI16Imm: return "LoadStaticI16Imm";
        case Opcode::LoadStaticU16Imm: return "LoadStaticU16Imm";
        case Opcode::LoadStaticI64Imm: return "LoadStaticI64Imm";
        case Opcode::LoadStaticU64Imm: return "LoadStaticU64Imm";
        case Opcode::LoadI8: return "LoadI8";
        case Opcode::LoadU8: return "LoadU8";
        case Opcode::LoadI16: return "LoadI16";
        case Opcode::LoadU16: return "LoadU16";
        case Opcode::LoadI32: return "LoadI32";
        case Opcode::LoadU32: return "LoadU32";
        case Opcode::LoadI64: return "LoadI64";
        case Opcode::LoadU64: return "LoadU64";
        case Opcode::StoreI8: return "StoreI8";
        case Opcode::StoreU8: return "StoreU8";
        case Opcode::StoreI16: return "StoreI16";
        case Opcode::StoreU16: return "StoreU16";
        case Opcode::StoreI32: return "StoreI32";
        case Opcode::StoreU32: return "StoreU32";
        case Opcode::StoreI64: return "StoreI64";
        case Opcode::StoreU64: return "StoreU64";
        case Opcode::INotEqual: return "INotEqual";
        case Opcode::FNotEqual: return "FNotEqual";
        case Opcode::StoreArray: return "StoreArray";
        case Opcode::AssertPointer: return "AssertPointer";
        case Opcode::BreakFlag: return "BreakFlag";
        case Opcode::Breakpoint: return "Breakpoint";
        default: return "Unknown Opcode";
        }
    }

    /// @brief One VM instruction. `padding` is the number of trailing alignment bytes
    ///        (4 for the T2R/T1X format, 0 for the compact UC4 format).
    /// @details The first 4 bytes are a union that can be viewed as:
    ///            - { opcode, a, b, c }         — general register form
    ///            - { opcode, a_imm, imm16 }    — signed 16-bit immediate form
    ///            - { opcode, a_k, uim16 }      — unsigned 16-bit immediate form
    ///            - { opcode, destination, operand1, operand2 } — raw bytes
    template <u8 padding> struct UP_Instruction {
        union {
            u32 as_u32; ///< whole instruction as a single u32

            struct {
                Opcode opcode : 8; ///< opcode
                u8     a;          ///< destination register / condition register
                u8     b;          ///< source register 1
                u8     c;          ///< source register 2
            };
            struct {
                u8 : 8;    ///< opcode padding
                u8  a_imm; ///< destination register for immediate form
                i16 imm16; ///< 16-bit signed immediate
            };
            struct {
                u8 : 8;    ///< opcode padding
                u8  a_k;   ///< destination register
                u16 uim16; ///< 16-bit unsigned immediate
            };
            struct {
                u8 : 8;         ///< opcode padding
                u8 destination; ///< raw destination byte
                u8 operand1;    ///< raw operand-1 byte
                u8 operand2;    ///< raw operand-2 byte
            };
        };

        std::array<std::byte, padding> m_padding; ///< trailing alignment bytes (unused by VM)

        [[nodiscard]] bool operator==(const UP_Instruction<padding> &rhs) const noexcept = default;

        [[nodiscard]] bool destination_is_immediate() const noexcept;
        [[nodiscard]] bool operand1_is_immediate() const noexcept;
        [[nodiscard]] bool operand2_is_immediate() const noexcept;
        [[nodiscard]] bool operand1_is_used() const noexcept;
        [[nodiscard]] bool operand2_is_used() const noexcept;
        [[nodiscard]] bool op1_is_reg() const noexcept;
        [[nodiscard]] bool op2_is_reg() const noexcept;

        /// @brief Pack a 16-bit value into operand1 (low byte) and operand2 (high byte).
        void set_lo_hi(const u16 value) noexcept {
            operand1 = static_cast<u8>(value & 0xFF);
            operand2 = static_cast<u8>((value >> 8) & 0xFF);
        }

        [[nodiscard]] const char *opcode_to_string() const noexcept;
        [[nodiscard]] std::string to_string() const { return opcode_to_string(); }
    };

    template <> struct UP_Instruction<0> {
        union {
            u32 as_u32; ///< whole instruction as a single u32

            struct {
                Opcode opcode : 8; ///< opcode
                u8     a;          ///< destination register / condition register
                u8     b;          ///< source register 1
                u8     c;          ///< source register 2
            };
            struct {
                u8 : 8;    ///< opcode padding
                u8  a_imm; ///< destination register for immediate form
                i16 imm16; ///< 16-bit signed immediate
            };
            struct {
                u8 : 8;    ///< opcode padding
                u8  a_k;   ///< destination register
                u16 uim16; ///< 16-bit unsigned immediate
            };
            struct {
                u8 : 8;         ///< opcode padding
                u8 destination; ///< raw destination byte
                u8 operand1;    ///< raw operand-1 byte
                u8 operand2;    ///< raw operand-2 byte
            };
        };

        [[nodiscard]] bool operator==(const UP_Instruction<0> &rhs) const noexcept = default;

        [[nodiscard]] bool destination_is_immediate() const noexcept;
        [[nodiscard]] bool operand1_is_immediate() const noexcept;
        [[nodiscard]] bool operand2_is_immediate() const noexcept;
        [[nodiscard]] bool operand1_is_used() const noexcept;
        [[nodiscard]] bool operand2_is_used() const noexcept;
        [[nodiscard]] bool op1_is_reg() const noexcept;
        [[nodiscard]] bool op2_is_reg() const noexcept;

        /// @brief Pack a 16-bit value into operand1 (low byte) and operand2 (high byte).
        void set_lo_hi(const u16 value) noexcept {
            operand1 = static_cast<u8>(value & 0xFF);
            operand2 = static_cast<u8>((value >> 8) & 0xFF);
        }

        [[nodiscard]] const char *opcode_to_string() const noexcept;
        [[nodiscard]] std::string to_string() const { return opcode_to_string(); }
    };

    template <u8 padding>
    [[nodiscard]] const char *UP_Instruction<padding>::opcode_to_string() const noexcept {
        return opcode_mnemonic(opcode);
    }

    [[nodiscard]] inline const char *UP_Instruction<0>::opcode_to_string() const noexcept {
        return opcode_mnemonic(opcode);
    }

    /// @brief Return true if the opcode writes to memory through a pointer.
    [[nodiscard]] static constexpr bool is_store_opcode(const Opcode op) noexcept {
        return op == Opcode::StoreI8 || op == Opcode::StoreU8 || op == Opcode::StoreI16 ||
               op == Opcode::StoreU16 || op == Opcode::StoreI32 || op == Opcode::StoreU32 ||
               op == Opcode::StoreI64 || op == Opcode::StoreU64 || op == Opcode::StoreFloat ||
               op == Opcode::StorePointer || op == Opcode::StoreArray;
    }

    // ---------------------------------------------------------------------------
    // UP_Instruction<padding> — out-of-line member definitions
    // ---------------------------------------------------------------------------

    template <u8 padding>
    [[nodiscard]] bool UP_Instruction<padding>::destination_is_immediate() const noexcept {
        return false;
    }

    template <u8 padding>
    [[nodiscard]] bool UP_Instruction<padding>::operand1_is_immediate() const noexcept {
        const auto *info = get_instruction_info(opcode);
        return info && info->is_imm(info->b_type);
    }

    template <u8 padding>
    [[nodiscard]] bool UP_Instruction<padding>::operand2_is_immediate() const noexcept {
        const auto *info = get_instruction_info(opcode);
        return info && info->is_imm(info->c_type);
    }

    template <u8 padding>
    [[nodiscard]] bool UP_Instruction<padding>::operand1_is_used() const noexcept {
        const auto *info = get_instruction_info(opcode);
        return info && info->is_used(info->b_type);
    }

    template <u8 padding>
    [[nodiscard]] bool UP_Instruction<padding>::operand2_is_used() const noexcept {
        const auto *info = get_instruction_info(opcode);
        return info && info->is_used(info->c_type);
    }

    template <u8 padding> [[nodiscard]] bool UP_Instruction<padding>::op1_is_reg() const noexcept {
        return operand1_is_used() && !operand1_is_immediate() && operand1 < ARG_REGISTERS_OFFSET;
    }

    template <u8 padding> [[nodiscard]] bool UP_Instruction<padding>::op2_is_reg() const noexcept {
        return operand2_is_used() && !operand2_is_immediate() && operand2 < ARG_REGISTERS_OFFSET;
    }

    // ---------------------------------------------------------------------------
    // UP_Instruction<0> — out-of-line member definitions
    // ---------------------------------------------------------------------------

    [[nodiscard]] inline bool UP_Instruction<0>::destination_is_immediate() const noexcept {
        return false;
    }

    [[nodiscard]] inline bool UP_Instruction<0>::operand1_is_immediate() const noexcept {
        const auto *info = get_instruction_info(opcode);
        return info && info->is_imm(info->b_type);
    }

    [[nodiscard]] inline bool UP_Instruction<0>::operand2_is_immediate() const noexcept {
        const auto *info = get_instruction_info(opcode);
        return info && info->is_imm(info->c_type);
    }

    [[nodiscard]] inline bool UP_Instruction<0>::operand1_is_used() const noexcept {
        const auto *info = get_instruction_info(opcode);
        return info && info->is_used(info->b_type);
    }

    [[nodiscard]] inline bool UP_Instruction<0>::operand2_is_used() const noexcept {
        const auto *info = get_instruction_info(opcode);
        return info && info->is_used(info->c_type);
    }

    [[nodiscard]] inline bool UP_Instruction<0>::op1_is_reg() const noexcept {
        return operand1_is_used() && !operand1_is_immediate() && operand1 < ARG_REGISTERS_OFFSET;
    }

    [[nodiscard]] inline bool UP_Instruction<0>::op2_is_reg() const noexcept {
        return operand2_is_used() && !operand2_is_immediate() && operand2 < ARG_REGISTERS_OFFSET;
    }

    // ---------------------------------------------------------------------------
    // Aliases and helpers
    // ---------------------------------------------------------------------------
    /*
     * ## Что означают эти сокращения
     *
     * | Код     | Игра                           | Год  | Платформа   | Формат инструкций |
     * |---------|--------------------------------|------|-------------|----------------------------|
     * | **UC1** | Uncharted: Drake's Fortune     | 2007 | PS3         | **другой** формат (не DC?)
     * | | **UC2** | Uncharted 2: Among Thieves     | 2009 | PS3         | **ввелись StateScript'ы**
     * | | **UC3** | Uncharted 3: Drake's Deception | 2011 | PS3         | близок к UC2 | | **UC4**
     * | Uncharted 4: A Thief's End     | 2016 | PS4         | **4-байтовые**             | |
     * **T1X** | Uncharted: The Lost Legacy     | 2017 | PS4         | **4-байтовые**             |
     * | **T2R** | The Last of Us Part II         | 2020 | PS4/PS5/ PC | **4-байтовые** |
     *
     * **Все три** (`UC4`, `T1X`, `T2R`) — **4-байтовые** инструкции. Это **единый формат**
     * DC-файлов, введённый в UC4 и используемый дальше.
     *
     * **`LogInstruction`** (8 байт) — **не используется** в реальных играх. Это **артефакт** нашего
     * кода: кто-то (или я) определил его как «T2R/T1X format», но это **неверно**.
     */
    using LongInstruction = UP_Instruction<4>; ///< 8-byte instruction (unused in practice)
    using ShortInstruction =
        UP_Instruction<0>;                ///< 4-byte instruction (canonical UC4/T1X/T2R format)
    using Instruction = ShortInstruction; ///< alias: canonical 4-byte instruction

    static_assert(sizeof(ShortInstruction) == 4, "ShortInstruction must be 4 bytes");
    static_assert(sizeof(LongInstruction) == 8, "LogInstruction must be 8 bytes");
    static_assert(sizeof(Instruction) == 4, "Instruction must be 4 bytes");

    /// @brief Expand a compact 4-byte instruction into the 8-byte form.
    [[nodiscard]] static constexpr LongInstruction
    from_short(const ShortInstruction &short_ins) noexcept {
        LongInstruction ins{};
        ins.opcode = short_ins.opcode;
        ins.destination = short_ins.destination;
        ins.operand1 = short_ins.operand1;
        ins.operand2 = short_ins.operand2;
        return ins;
    }

    template <u8 padding>
    inline std::ostream &operator<<(std::ostream &os, const UP_Instruction<padding> &ins) noexcept {
        os << ins.opcode_to_string() << " " << static_cast<unsigned>(ins.destination) << " "
           << static_cast<unsigned>(ins.operand1) << " " << static_cast<unsigned>(ins.operand2);
        return os;
    }

    // ---------------------------------------------------------------------------
    // Disassembled line
    // ---------------------------------------------------------------------------

    using istr_line = u16;

    /// @brief One decoded instruction line inside a disassembled function.
    struct function_disassembly_line {
        Instruction        m_instruction;             ///< decoded instruction (4 bytes)
        istr_line          m_location;                ///< instruction index within the function
        std::string        m_text;                    ///< formatted disassembly text
        const Instruction *m_globalPointer = nullptr; ///< start of the function's instruction array
        std::string        m_comment;                 ///< formatted comment (side info)
        u16  m_target = std::numeric_limits<u16>::max(); ///< branch target index (max == no target)
        bool m_isArgMove = false;                        ///< true if this is an argument move

        function_disassembly_line() noexcept = default;

        function_disassembly_line(u64 idx, const Instruction *ptr) noexcept
            : m_instruction(ptr[idx]), m_location(idx), m_globalPointer(ptr) {}
    };

    // ---------------------------------------------------------------------------
    // Instruction factory
    // ---------------------------------------------------------------------------

    /// @brief Convenience constructors for Instruction values.
    class InstructionFactory {
    public:
        /// @brief Three-register instruction (dest, op1, op2).
        [[nodiscard]] static constexpr Instruction abc(Opcode op, u8 a, u8 b, u8 c) noexcept {
            Instruction ins{};
            ins.opcode = op;
            ins.destination = a;
            ins.operand1 = b;
            ins.operand2 = c;
            return ins;
        }

        /// @brief Two-register instruction (dest, op1); operand2 = 0.
        [[nodiscard]] static constexpr Instruction ab(Opcode op, u8 a, u8 b) noexcept {
            return abc(op, a, b, 0);
        }

        /// @brief One-register instruction (dest).
        [[nodiscard]] static constexpr Instruction a(Opcode op, u8 a) noexcept {
            return abc(op, a, 0, 0);
        }

        /// @brief Instruction with a 16-bit immediate (imm16) in operand1/operand2.
        [[nodiscard]] static constexpr Instruction imm(Opcode op, u8 a, u16 imm) noexcept {
            Instruction ins{};
            ins.opcode = op;
            ins.destination = a;
            ins.set_lo_hi(imm);
            return ins;
        }

        /// @brief Branch instruction with a 16-bit target instruction index.
        [[nodiscard]] static constexpr Instruction branch(Opcode op, u16 target) noexcept {
            Instruction ins{};
            ins.opcode = op;
            ins.set_lo_hi(target);
            return ins;
        }

        /// @brief Lookup instruction with a 16-bit symbol-table index.
        [[nodiscard]] static constexpr Instruction lookup(Opcode op, u8 dst,
                                                          u16 sym_offset) noexcept {
            Instruction ins{};
            ins.opcode = op;
            ins.destination = dst;
            ins.set_lo_hi(sym_offset);
            return ins;
        }
    };

    // ---------------------------------------------------------------------------
    // Instruction metadata table
    // ---------------------------------------------------------------------------

    // IMPORTANT: this table MUST be in the same order as `enum class Opcode`,
    // because `get_instruction_info` uses binary search over `opcode`.
    static const InstructionInfo INSTRUCTION_INFO[] = {
        {Opcode::Return, "Return", OperandType::REG, OperandType::NONE, OperandType::NONE,
         StaticType::NONE},
        {Opcode::IAdd, "IAdd", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::ISub, "ISub", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::IMul, "IMul", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::IDiv, "IDiv", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::FAdd, "FAdd", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::FSub, "FSub", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::FMul, "FMul", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::FDiv, "FDiv", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::LoadStaticInt, "LoadStaticInt", OperandType::REG, OperandType::REG,
         OperandType::NONE, StaticType::I32},
        {Opcode::LoadStaticFloat, "LoadStaticFloat", OperandType::REG, OperandType::REG,
         OperandType::NONE, StaticType::FLOAT},
        {Opcode::LoadStaticPointer, "LoadStaticPointer", OperandType::REG, OperandType::REG,
         OperandType::NONE, StaticType::POINTER},
        {Opcode::LoadU16Imm, "LoadU16Imm", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::NONE},
        {Opcode::LoadInt, "LoadU32", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::LoadFloat, "LoadFloat", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::LoadPointer, "LoadPointer", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::StoreInt, "StoreInt", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::StoreFloat, "StoreFloat", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::StorePointer, "StorePointer", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::LookupInt, "LookupInt", OperandType::REG, OperandType::IMM_U16, OperandType::NONE,
         StaticType::SID},
        {Opcode::LookupFloat, "LookupFloat", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::SID},
        {Opcode::LookupPointer, "LookupPointer", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::SID},
        {Opcode::MoveInt, "MoveInt", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::MoveFloat, "MoveFloat", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::MovePointer, "MovePointer", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::CastInteger, "CastInteger", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::CastFloat, "CastFloat", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::Call, "Call", OperandType::REG, OperandType::REG, OperandType::IMM_U8,
         StaticType::NONE},
        {Opcode::CallFf, "CallFf", OperandType::REG, OperandType::REG, OperandType::IMM_U8,
         StaticType::NONE},
        {Opcode::IEqual, "IEqual", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::IGreaterThan, "IGreaterThan", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::IGreaterThanEqual, "IGreaterThanEqual", OperandType::REG, OperandType::REG,
         OperandType::REG, StaticType::NONE},
        {Opcode::ILessThan, "ILessThan", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::ILessThanEqual, "ILessThanEqual", OperandType::REG, OperandType::REG,
         OperandType::REG, StaticType::NONE},
        {Opcode::FEqual, "FEqual", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::FGreaterThan, "FGreaterThan", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::FGreaterThanEqual, "FGreaterThanEqual", OperandType::REG, OperandType::REG,
         OperandType::REG, StaticType::NONE},
        {Opcode::FLessThan, "FLessThan", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::FLessThanEqual, "FLessThanEqual", OperandType::REG, OperandType::REG,
         OperandType::REG, StaticType::NONE},
        {Opcode::IMod, "IMod", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::FMod, "FMod", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::IAbs, "IAbs", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::FAbs, "FAbs", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::GoTo, "GoTo", OperandType::NONE, OperandType::NONE, OperandType::NONE,
         StaticType::NONE},
        {Opcode::Label, "Label", OperandType::NONE, OperandType::NONE, OperandType::NONE,
         StaticType::NONE},
        {Opcode::Branch, "Branch", OperandType::NONE, OperandType::IMM_I16, OperandType::NONE,
         StaticType::NONE},
        {Opcode::BranchIf, "BranchIf", OperandType::REG, OperandType::IMM_I16, OperandType::NONE,
         StaticType::NONE},
        {Opcode::BranchIfNot, "BranchIfNot", OperandType::REG, OperandType::IMM_I16,
         OperandType::NONE, StaticType::NONE},
        {Opcode::OpLogNot, "OpLogNot", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::OpBitAnd, "OpBitAnd", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::OpBitNot, "OpBitNot", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::OpBitOr, "OpBitOr", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::OpBitXor, "OpBitXor", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::OpBitNor, "OpBitNor", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::OpLogAnd, "OpLogAnd", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::OpLogOr, "OpLogOr", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::INeg, "INeg", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::FNeg, "FNeg", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::LoadParamCnt, "LoadParamCnt", OperandType::REG, OperandType::NONE,
         OperandType::NONE, StaticType::NONE},
        {Opcode::IAddImm, "IAddImm", OperandType::REG, OperandType::REG, OperandType::IMM_I16,
         StaticType::NONE},
        {Opcode::ISubImm, "ISubImm", OperandType::REG, OperandType::REG, OperandType::IMM_I16,
         StaticType::NONE},
        {Opcode::IMulImm, "IMulImm", OperandType::REG, OperandType::REG, OperandType::IMM_I16,
         StaticType::NONE},
        {Opcode::IDivImm, "IDivImm", OperandType::REG, OperandType::REG, OperandType::IMM_I16,
         StaticType::NONE},
        {Opcode::LoadStaticI32Imm, "LoadStaticI32Imm", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::I32},
        {Opcode::LoadStaticFloatImm, "LoadStaticFloatImm", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::FLOAT},
        {Opcode::LoadStaticPointerImm, "LoadStaticPointerImm", OperandType::REG,
         OperandType::IMM_U16, OperandType::NONE, StaticType::POINTER},
        {Opcode::IntAsh, "IntAsh", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::Move, "Move", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::LoadStaticU32Imm, "LoadStaticU32Imm", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::U32},
        {Opcode::LoadStaticI8Imm, "LoadStaticI8Imm", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::I8},
        {Opcode::LoadStaticU8Imm, "LoadStaticU8Imm", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::U8},
        {Opcode::LoadStaticI16Imm, "LoadStaticI16Imm", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::I16},
        {Opcode::LoadStaticU16Imm, "LoadStaticU16Imm", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::U16},
        {Opcode::LoadStaticI64Imm, "LoadStaticI64Imm", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::I64},
        {Opcode::LoadStaticU64Imm, "LoadStaticU64Imm", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::U64},
        {Opcode::LoadI8, "LoadI8", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::LoadU8, "LoadU8", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::LoadI16, "LoadI16", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::LoadU16, "LoadU16", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::LoadI32, "LoadI32", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::LoadU32, "LoadU32", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::LoadI64, "LoadI64", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::LoadU64, "LoadU64", OperandType::REG, OperandType::REG, OperandType::NONE,
         StaticType::NONE},
        {Opcode::StoreI8, "StoreI8", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::StoreU8, "StoreU8", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::StoreI16, "StoreI16", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::StoreU16, "StoreU16", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::StoreI32, "StoreI32", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::StoreU32, "StoreU32", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::StoreI64, "StoreI64", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::StoreU64, "StoreU64", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::INotEqual, "INotEqual", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::FNotEqual, "FNotEqual", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::StoreArray, "StoreArray", OperandType::REG, OperandType::REG, OperandType::REG,
         StaticType::NONE},
        {Opcode::AssertPointer, "AssertPointer", OperandType::REG, OperandType::NONE,
         OperandType::NONE, StaticType::NONE},
        {Opcode::BreakFlag, "BreakFlag", OperandType::NONE, OperandType::NONE, OperandType::NONE,
         StaticType::NONE},
        {Opcode::Breakpoint, "Breakpoint", OperandType::NONE, OperandType::NONE, OperandType::NONE,
         StaticType::NONE},
    };

    /// @brief Look up instruction metadata by opcode.
    /// @details INSTRUCTION_INFO is in the same order as `enum class Opcode`,
    ///          so this is a direct indexed lookup.
    [[nodiscard]] inline const InstructionInfo *get_instruction_info(Opcode op) {
        const auto idx = static_cast<size_t>(op);
        if (idx >= std::size(INSTRUCTION_INFO)) { return nullptr; }
        return &INSTRUCTION_INFO[idx];
    }

} // namespace carbon