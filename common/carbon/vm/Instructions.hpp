#pragma once

#include "CommonTypes.hpp"
#include "Opcodes.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <ostream>
#include <string>

namespace carbon {

    constexpr u32 MAX_REGISTERS = 34;
    constexpr u32 ARG_REGISTERS_OFFSET = 24;
    constexpr u32 LOCAL_REGISTERS_OFFSET = 0;
    constexpr u32 MAX_LOCALS = ARG_REGISTERS_OFFSET - LOCAL_REGISTERS_OFFSET;
    constexpr u32 MAX_ARGS = MAX_REGISTERS - ARG_REGISTERS_OFFSET;

    enum class OperandType : u8 { NONE, REG, IMM_U8, IMM_I16, IMM_U16 };
    enum class StaticType {
        NONE,
        POINTER,
        I8,
        U8,
        I16,
        U16,
        I32,
        U32,
        I64,
        U64,
        FLOAT,
        DOUBLE,
        SID
    };

    struct InstructionInfo {
        Opcode      opcode;
        const char *name;
        OperandType a_type;
        OperandType b_type;
        OperandType c_type;
        StaticType  static_type;

        [[nodiscard]] size_t operands_count() const noexcept {
            size_t n = 0;
            if (a_type != OperandType::NONE) ++n;
            if (b_type != OperandType::NONE) ++n;
            if (c_type != OperandType::NONE) ++n;
            return n;
        }
        [[nodiscard]] bool is_reg(OperandType t) const noexcept { return t == OperandType::REG; }
        [[nodiscard]] bool is_imm(OperandType t) const noexcept {
            return t == OperandType::IMM_U8 || t == OperandType::IMM_I16 ||
                   t == OperandType::IMM_U16;
        }
        [[nodiscard]] bool is_used(OperandType t) const noexcept { return t != OperandType::NONE; }
    };

    [[nodiscard]] inline const InstructionInfo *get_instruction_info(Opcode op);
    [[nodiscard]] inline const char            *opcode_mnemonic(Opcode opcode) noexcept;

    // -------------------------------------------------------------------------
    // UP_Instruction
    // -------------------------------------------------------------------------
    //
    //   byte 0 : opcode
    //   byte 1 : a
    //   byte 2 : b
    //   byte 3 : c
    //   bytes 4..4+Padding : padding (unused)
    //
    // imm16 is computed from b (low) and c (high).

    template <u8 Padding> struct UP_Instruction {
        Opcode                  opcode;
        u8                      a;
        u8                      b;
        u8                      c;
        std::array<u8, Padding> padding;

        [[nodiscard]] u16 imm16() const noexcept {
            return static_cast<u16>(b) | (static_cast<u16>(c) << 8);
        }
        [[nodiscard]] i16 imm16_signed() const noexcept { return static_cast<i16>(imm16()); }
        void              set_imm16(u16 v) noexcept {
            b = static_cast<u8>(v & 0xFF);
            c = static_cast<u8>(v >> 8);
        }

        [[nodiscard]] bool operand1_is_immediate() const noexcept {
            const auto *info = get_instruction_info(opcode);
            return info && info->is_imm(info->b_type);
        }
        [[nodiscard]] bool operand2_is_immediate() const noexcept {
            const auto *info = get_instruction_info(opcode);
            return info && info->is_imm(info->c_type);
        }
        [[nodiscard]] bool operand1_is_used() const noexcept {
            const auto *info = get_instruction_info(opcode);
            return info && info->is_used(info->b_type);
        }
        [[nodiscard]] bool operand2_is_used() const noexcept {
            const auto *info = get_instruction_info(opcode);
            return info && info->is_used(info->c_type);
        }
        [[nodiscard]] bool op1_is_reg() const noexcept {
            return operand1_is_used() && !operand1_is_immediate() && b < ARG_REGISTERS_OFFSET;
        }
        [[nodiscard]] bool op2_is_reg() const noexcept {
            return operand2_is_used() && !operand2_is_immediate() && c < ARG_REGISTERS_OFFSET;
        }

        [[nodiscard]] const char *opcode_to_string() const noexcept {
            return opcode_mnemonic(opcode);
        }
        [[nodiscard]] std::string to_string() const { return opcode_to_string(); }

        [[nodiscard]] bool operator==(const UP_Instruction &rhs) const noexcept {
            return opcode == rhs.opcode && a == rhs.a && b == rhs.b && c == rhs.c;
        }
    };

    // Specialisation for the canonical 4-byte form: no trailing padding.
    template <> struct UP_Instruction<0> {
        Opcode opcode;
        u8     a;
        u8     b;
        u8     c;

        [[nodiscard]] u16 imm16() const noexcept {
            return static_cast<u16>(b) | (static_cast<u16>(c) << 8);
        }
        [[nodiscard]] i16 imm16_signed() const noexcept { return static_cast<i16>(imm16()); }
        void              set_imm16(u16 v) noexcept {
            b = static_cast<u8>(v & 0xFF);
            c = static_cast<u8>(v >> 8);
        }

        [[nodiscard]] bool operand1_is_immediate() const noexcept {
            const auto *info = get_instruction_info(opcode);
            return info && info->is_imm(info->b_type);
        }
        [[nodiscard]] bool operand2_is_immediate() const noexcept {
            const auto *info = get_instruction_info(opcode);
            return info && info->is_imm(info->c_type);
        }
        [[nodiscard]] bool operand1_is_used() const noexcept {
            const auto *info = get_instruction_info(opcode);
            return info && info->is_used(info->b_type);
        }
        [[nodiscard]] bool operand2_is_used() const noexcept {
            const auto *info = get_instruction_info(opcode);
            return info && info->is_used(info->c_type);
        }
        [[nodiscard]] bool op1_is_reg() const noexcept {
            return operand1_is_used() && !operand1_is_immediate() && b < ARG_REGISTERS_OFFSET;
        }
        [[nodiscard]] bool op2_is_reg() const noexcept {
            return operand2_is_used() && !operand2_is_immediate() && c < ARG_REGISTERS_OFFSET;
        }

        [[nodiscard]] const char *opcode_to_string() const noexcept {
            return opcode_mnemonic(opcode);
        }
        [[nodiscard]] std::string to_string() const { return opcode_to_string(); }

        [[nodiscard]] bool operator==(const UP_Instruction<0> &rhs) const noexcept {
            return opcode == rhs.opcode && a == rhs.a && b == rhs.b && c == rhs.c;
        }
    };

    using LongInstruction = UP_Instruction<4>;
    using ShortInstruction = UP_Instruction<0>;
    using Instruction = ShortInstruction;

    static_assert(sizeof(ShortInstruction) == 4, "ShortInstruction must be 4 bytes");
    static_assert(sizeof(LongInstruction) == 8, "LongInstruction must be 8 bytes");
    static_assert(sizeof(Instruction) == 4, "Instruction must be 4 bytes");

    [[nodiscard]] inline LongInstruction from_short(const ShortInstruction &s) noexcept {
        LongInstruction out{};
        out.opcode = s.opcode;
        out.a = s.a;
        out.b = s.b;
        out.c = s.c;
        return out;
    }

    inline std::ostream &operator<<(std::ostream &os, const Instruction &ins) noexcept {
        os << ins.opcode_to_string() << " " << unsigned(ins.a) << " " << unsigned(ins.b) << " "
           << unsigned(ins.c);
        return os;
    }

    // -------------------------------------------------------------------------
    // InstructionFactory
    // -------------------------------------------------------------------------

    class InstructionFactory {
    public:
        [[nodiscard]] static constexpr Instruction abc(Opcode op, u8 a, u8 b, u8 c) noexcept {
            return Instruction{op, a, b, c};
        }
        [[nodiscard]] static constexpr Instruction ab(Opcode op, u8 a, u8 b) noexcept {
            return abc(op, a, b, 0);
        }
        [[nodiscard]] static constexpr Instruction a(Opcode op, u8 a) noexcept {
            return abc(op, a, 0, 0);
        }
        [[nodiscard]] static constexpr Instruction imm(Opcode op, u8 a, u16 imm) noexcept {
            Instruction ins{op, a, 0, 0};
            ins.set_imm16(imm);
            return ins;
        }
        [[nodiscard]] static constexpr Instruction branch(Opcode op, u16 target) noexcept {
            Instruction ins{op, 0, 0, 0};
            ins.set_imm16(target);
            return ins;
        }
        [[nodiscard]] static constexpr Instruction lookup(Opcode op, u8 dst, u16 sym) noexcept {
            Instruction ins{op, dst, 0, 0};
            ins.set_imm16(sym);
            return ins;
        }
    };

    // -------------------------------------------------------------------------
    // opcode_mnemonic
    // -------------------------------------------------------------------------

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
        case Opcode::CallMethod: return "CallMethod";
        default: return "Unknown Opcode";
        }
    }

    // -------------------------------------------------------------------------
    // INSTRUCTION_INFO
    // -------------------------------------------------------------------------

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
        {Opcode::LoadStaticInt, "LoadStaticInt", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::I32},
        {Opcode::LoadStaticFloat, "LoadStaticFloat", OperandType::REG, OperandType::IMM_U16,
         OperandType::NONE, StaticType::FLOAT},
        {Opcode::LoadStaticPointer, "LoadStaticPointer", OperandType::REG, OperandType::IMM_U16,
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
        {Opcode::CallMethod, "CallMethod", OperandType::REG, OperandType::REG, OperandType::IMM_U8,
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
        {Opcode::IAddImm, "IAddImm", OperandType::REG, OperandType::REG, OperandType::IMM_U8,
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

    [[nodiscard]] inline const InstructionInfo *get_instruction_info(Opcode op) {
        const auto idx = static_cast<size_t>(op);
        if (idx >= std::size(INSTRUCTION_INFO)) return nullptr;
        return &INSTRUCTION_INFO[idx];
    }

} // namespace carbon