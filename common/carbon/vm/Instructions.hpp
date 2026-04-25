#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "Opcodes.hpp"
#include <cstddef>
#include <format>
#include <string>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <array>
#include <limits>
#include <ostream>

using namespace carbon;


namespace carbon {



enum class OperandType : u8 {
    NONE,
    REG,          // регистр
    IMM_U8,          // 8-ише 
    IMM_I16,      // 16-bit signed immediate
    IMM_U16,      // 16-bit unsigned (индекс ST, аргументы)
};

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
    Opcode opcode;
    const char* name;
    OperandType a_type;  // тип первого операнда
    OperandType b_type;  // тип второго операнда
    OperandType c_type;  // тип третьего операнда
    StaticType  static_type; // тип константы в таблице
    
    size_t oprands_count() const { 
        size_t cnt = 0;
        if (a_type != OperandType::NONE) cnt++;
        if (b_type != OperandType::NONE) cnt++;
        if (c_type != OperandType::NONE) cnt++;
        return cnt;
    }
    bool is_reg(OperandType t) const { return t == OperandType::REG; }
    bool is_imm(OperandType t) const { return t == OperandType::IMM_I16 || t == OperandType::IMM_U16; }
    bool is_used(OperandType t) const { return t != OperandType::NONE; }
};

inline const InstructionInfo* get_instruction_info(Opcode op);

template<u8 padding>
struct UP_Instruction {
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
            u16 uim16;     // 16-bit unsigned immediate
        };

        
        struct {
            u8 : 8;        // padding for opcode  
            u8 destination;
            u8 operand1;
            u8 operand2;
        };
    };
            
    std::array<std::byte, padding> m_padding;

    [[nodiscard]] bool operator==(const UP_Instruction<padding>& rhs) const noexcept = default;

    [[nodiscard]] bool destination_is_immediate() const noexcept;

    [[nodiscard]] bool operand1_is_immediate() const noexcept;
    [[nodiscard]] bool operand2_is_immediate() const noexcept;
    [[nodiscard]] bool operand1_is_used() const noexcept;
    [[nodiscard]] bool operand2_is_used() const noexcept;

    [[nodiscard]] bool op1_is_reg() const noexcept;
    [[nodiscard]] bool op2_is_reg() const noexcept;

    void set_lo_hi(const u16 value) noexcept {
        destination = value & 0xFF;
        operand2 = (value >> 8) & 0xFF;
    }


    const char* opcode_to_string() const noexcept;

    std::string to_string() { return opcode_to_string(); }
};

template<>
struct UP_Instruction<0> {
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
            u16 uim16;     // 16-bit unsigned immediate
        };

        
        struct {
            u8 : 8;        // padding for opcode  
            u8 destination;
            u8 operand1;
            u8 operand2;
        };
    };

    [[nodiscard]] bool operator==(const UP_Instruction<0>& rhs) const noexcept = default;

    [[nodiscard]] bool destination_is_immediate() const noexcept;


    [[nodiscard]] bool operand1_is_immediate() const noexcept;
    [[nodiscard]] bool operand2_is_immediate() const noexcept;
    [[nodiscard]] bool operand1_is_used() const noexcept;
    [[nodiscard]] bool operand2_is_used() const noexcept;

    [[nodiscard]] bool op1_is_reg() const noexcept;
    [[nodiscard]] bool op2_is_reg() const noexcept;


    const char* opcode_to_string() const noexcept;
};

template<u8 padding>
const char* UP_Instruction<padding>::opcode_to_string() const noexcept {
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

[[nodiscard]] static constexpr bool is_store_opcode(const Opcode op) {
    return 
        op == Opcode::StoreI8 ||
        op == Opcode::StoreU8 ||
        op == Opcode::StoreI16 ||
        op == Opcode::StoreU16 ||
        op == Opcode::StoreI32 ||
        op == Opcode::StoreU32 ||
        op == Opcode::StoreI64 ||
        op == Opcode::StoreU64 ||
        op == Opcode::StoreFloat ||
        op == Opcode::StorePointer ||
        op == Opcode::StoreArray;
}

template<u8 padding>
[[nodiscard]] bool UP_Instruction<padding>::destination_is_immediate() const noexcept {
    return false;
}
template<u8 padding>
[[nodiscard]] bool UP_Instruction<padding>::operand1_is_immediate() const noexcept {
    auto* info = get_instruction_info(opcode);
    return  info->b_type == OperandType::IMM_U8 || info->b_type == OperandType::IMM_U16 || info->b_type == OperandType::IMM_I16;
}
template<u8 padding>
[[nodiscard]] bool UP_Instruction<padding>::operand2_is_immediate() const noexcept {
    auto* info = get_instruction_info(opcode);
    return  info->c_type == OperandType::IMM_U8;
}
template<u8 padding>
[[nodiscard]] bool UP_Instruction<padding>::operand1_is_used() const noexcept {
    auto* info = get_instruction_info(opcode);
    return info ? info->is_used(info->b_type) : false;
}
template<u8 padding>
[[nodiscard]] bool UP_Instruction<padding>::operand2_is_used() const noexcept {
    auto* info = get_instruction_info(opcode);
    return info ? info->is_used(info->c_type) : false;
}
template<u8 padding>
[[nodiscard]] bool UP_Instruction<padding>::op1_is_reg() const noexcept {
    return operand1_is_used() && !operand1_is_immediate() && operand1 < ARG_REGISTERS_OFFSET;
}
template<u8 padding>
[[nodiscard]] bool UP_Instruction<padding>::op2_is_reg() const noexcept {
    return operand2_is_used() && !operand2_is_immediate() && operand2 < ARG_REGISTERS_OFFSET;
}

using Instruction = UP_Instruction<4>;
using ShortInstruction = UP_Instruction<0>;

[[nodiscard]] static constexpr Instruction from_short(const ShortInstruction& short_ins) noexcept {
    Instruction ins;
    ins.opcode = short_ins.opcode;
    ins.destination = short_ins.destination;
    ins.operand1 = short_ins.operand1;
    ins.operand2 = short_ins.operand2;
    return ins;
}

static_assert(sizeof(Instruction) == 8);
static_assert(sizeof(ShortInstruction) == 4);

template<u8 padding>
inline std::ostream& operator<<(std::ostream& os, const UP_Instruction<padding>& ins) noexcept {
    os << ins.opcode_to_string() << " " << static_cast<unsigned>(ins.destination)
       << " " << static_cast<unsigned>(ins.operand1)
       << " " << static_cast<unsigned>(ins.operand2);
    return os;
}

#undef max

using istr_line = u16;

struct function_disassembly_line {
    Instruction m_instruction;
    istr_line m_location;
    std::string m_text;
    const Instruction* m_globalPointer;
    std::string m_comment;
    u16 m_target = std::numeric_limits<u16>::max();
    bool m_isArgMove;

    function_disassembly_line() noexcept = default;

    function_disassembly_line(u64 idx, const Instruction* ptr, const bool is_64_bit_instruction = true) noexcept :
        m_instruction(is_64_bit_instruction ? ptr[idx] : from_short(reinterpret_cast<const ShortInstruction*>(ptr)[idx])),
        m_location(idx),
        m_globalPointer(ptr),
        m_isArgMove(false)
    {}
};


class InstructionFactory {
public:
    static Instruction abc(Opcode op, u8 a, u8 b, u8 c) {
        Instruction ins;
        ins.opcode = op;
        ins.destination = a;
        ins.operand1 = b;
        ins.operand2 = c;
        return ins;
    }
    
    static Instruction ab(Opcode op, u8 a, u8 b) {
        return abc(op, a, b, 0);
    }
    
    static Instruction a(Opcode op, u8 a) {
        return abc(op, a, 0, 0);
    }
    
    static Instruction imm(Opcode op, u8 a, u16 imm) {
        Instruction ins;
        ins.opcode = op;
        ins.destination = a;
        ins.set_lo_hi(imm);  // ← используем ИХ метод!
        return ins;
    }
    
    static Instruction branch(Opcode op, u16 target) {
        Instruction ins;
        ins.opcode = op;
        ins.set_lo_hi(target);
        return ins;
    }
    
    static Instruction lookup(Opcode op, u8 dst, u16 sym_offset) {
        Instruction ins;
        ins.opcode = op;
        ins.destination = dst;
        ins.set_lo_hi(sym_offset);
        return ins;
    }
};



// Таблица ВСЕХ инструкций
static const InstructionInfo INSTRUCTION_INFO[] = {
    // opcode,          name,                    a_type,       b_type,       c_type
    { Opcode::Return,   "Return",               OperandType::REG, OperandType::NONE, OperandType::NONE, StaticType::NONE },
    { Opcode::IAdd,     "IAdd",                 OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::ISub,     "ISub",                 OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::IMul,     "IMul",                 OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::IDiv,     "IDiv",                 OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::IMod,     "IMod",                 OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::IAbs,     "IAbs",                 OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::INeg,     "INeg",                 OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::IntAsh,   "IntAsh",               OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    
    { Opcode::FAdd,     "FAdd",                 OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::FSub,     "FSub",                 OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::FMul,     "FMul",                 OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::FDiv,     "FDiv",                 OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::FMod,     "FMod",                 OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::FAbs,     "FAbs",                 OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::FNeg,     "FNeg",                 OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    
    { Opcode::Move,     "Move",                 OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::MoveInt,  "MoveInt",              OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::MoveFloat,"MoveFloat",            OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::MovePointer,"MovePointer",        OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    
    { Opcode::CastInteger,"CastInteger",        OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::CastFloat,"CastFloat",            OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    
    // Static Load (индекс в регистре)
    { Opcode::LoadStaticInt,    "LoadStaticInt",    OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::I32 },
    { Opcode::LoadStaticFloat,  "LoadStaticFloat",  OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::FLOAT },
    { Opcode::LoadStaticPointer,"LoadStaticPointer",OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::POINTER },
    
    // Static Load Immediate (индекс = operand1)
    { Opcode::LoadStaticI8Imm,  "LoadStaticI8Imm",  OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::I8 },
    { Opcode::LoadStaticU8Imm,  "LoadStaticU8Imm",  OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::U8 },
    { Opcode::LoadStaticI16Imm, "LoadStaticI16Imm", OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::I16 },
    { Opcode::LoadStaticU16Imm, "LoadStaticU16Imm", OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::U16 },
    { Opcode::LoadStaticI32Imm, "LoadStaticI32Imm", OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::I32 },
    { Opcode::LoadStaticU32Imm, "LoadStaticU32Imm", OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::U32 },
    { Opcode::LoadStaticI64Imm, "LoadStaticI64Imm", OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::I64 },
    { Opcode::LoadStaticU64Imm, "LoadStaticU64Imm", OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::U64 },
    { Opcode::LoadStaticFloatImm,"LoadStaticFloatImm",OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::FLOAT },
    { Opcode::LoadStaticPointerImm,"LoadStaticPointerImm",OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::POINTER },
    
    // Indirect Load (через указатель)
    { Opcode::LoadI8,   "LoadI8",               OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::LoadU8,   "LoadU8",               OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::LoadI16,  "LoadI16",              OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::LoadU16,  "LoadU16",              OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::LoadI32,  "LoadI32",              OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::LoadInt,  "LoadU32",              OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::LoadI64,  "LoadI64",              OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::LoadU64,  "LoadU64",              OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::LoadFloat,"LoadFloat",            OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::LoadPointer,"LoadPointer",        OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    { Opcode::LoadU16Imm,"LoadU16Imm",          OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::NONE },
    
    // Indirect Store (через указатель)
    { Opcode::StoreI8,  "StoreI8",              OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::StoreU8,  "StoreU8",              OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::StoreI16, "StoreI16",             OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::StoreU16, "StoreU16",             OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::StoreI32, "StoreI32",             OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::StoreU32, "StoreU32",             OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::StoreI64, "StoreI64",             OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::StoreU64, "StoreU64",             OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::StoreInt, "StoreInt",             OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::StoreFloat,"StoreFloat",          OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::StorePointer,"StorePointer",      OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::StoreArray,"StoreArray",          OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    
    // Lookup
    { Opcode::LookupInt,    "LookupInt",        OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::SID },
    { Opcode::LookupFloat,  "LookupFloat",      OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::SID },
    { Opcode::LookupPointer,"LookupPointer",    OperandType::REG, OperandType::IMM_U16, OperandType::NONE, StaticType::SID },
    
    // Comparisons
    { Opcode::IEqual,       "IEqual",           OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::INotEqual,    "INotEqual",        OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::IGreaterThan, "IGreaterThan",     OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::IGreaterThanEqual,"IGreaterThanEqual",OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::ILessThan,    "ILessThan",        OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::ILessThanEqual,"ILessThanEqual",  OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::FEqual,       "FEqual",           OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::FNotEqual,    "FNotEqual",        OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::FGreaterThan, "FGreaterThan",     OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::FGreaterThanEqual,"FGreaterThanEqual",OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::FLessThan,    "FLessThan",        OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::FLessThanEqual,"FLessThanEqual",  OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    
    // Logical
    { Opcode::OpLogAnd, "OpLogAnd",             OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::OpLogOr,  "OpLogOr",              OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::OpLogNot, "OpLogNot",             OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    
    // Bitwise
    { Opcode::OpBitAnd, "OpBitAnd",             OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE},
    { Opcode::OpBitOr,  "OpBitOr",              OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::OpBitXor, "OpBitXor",             OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::OpBitNor, "OpBitNor",             OperandType::REG, OperandType::REG, OperandType::REG, StaticType::NONE },
    { Opcode::OpBitNot, "OpBitNot",             OperandType::REG, OperandType::REG, OperandType::NONE, StaticType::NONE },
    
    // Immediate arithmetic
    { Opcode::IAddImm,  "IAddImm",              OperandType::REG, OperandType::REG, OperandType::IMM_I16, StaticType::NONE },
    { Opcode::ISubImm,  "ISubImm",              OperandType::REG, OperandType::REG, OperandType::IMM_I16, StaticType::NONE },
    { Opcode::IMulImm,  "IMulImm",              OperandType::REG, OperandType::REG, OperandType::IMM_I16, StaticType::NONE },
    { Opcode::IDivImm,  "IDivImm",              OperandType::REG, OperandType::REG, OperandType::IMM_I16, StaticType::NONE },
    
    // Branches
    { Opcode::Branch,       "Branch",           OperandType::NONE, OperandType::IMM_I16, OperandType::NONE, StaticType::NONE }, // imm16
    { Opcode::BranchIf,     "BranchIf",         OperandType::REG, OperandType::IMM_I16, OperandType::NONE, StaticType::NONE },  // + imm16
    { Opcode::BranchIfNot,  "BranchIfNot",      OperandType::REG, OperandType::IMM_I16, OperandType::NONE, StaticType::NONE },  // + imm16
    { Opcode::GoTo,         "GoTo",             OperandType::NONE, OperandType::NONE, OperandType::NONE, StaticType::NONE },
    { Opcode::Label,        "Label",            OperandType::NONE, OperandType::NONE, OperandType::NONE, StaticType::NONE },
    
    // Calls
    { Opcode::Call,         "Call",             OperandType::REG, OperandType::REG, OperandType::IMM_U8, StaticType::NONE },
    { Opcode::CallFf,       "CallFf",           OperandType::REG, OperandType::REG, OperandType::IMM_U8, StaticType::NONE },
    
    // Misc
    { Opcode::LoadParamCnt, "LoadParamCnt",     OperandType::REG, OperandType::NONE, OperandType::NONE, StaticType::NONE },
    
    // Debug
    { Opcode::AssertPointer,"AssertPointer",    OperandType::REG, OperandType::NONE, OperandType::NONE, StaticType::NONE },
    { Opcode::BreakFlag,    "BreakFlag",        OperandType::NONE, OperandType::NONE, OperandType::NONE, StaticType::NONE },
    { Opcode::Breakpoint,   "Breakpoint",       OperandType::NONE, OperandType::NONE, OperandType::NONE, StaticType::NONE },
};

inline const InstructionInfo* get_instruction_info(Opcode op) {
    for (const auto& info : INSTRUCTION_INFO) {
        if (info.opcode == op) return &info;
    }
    return nullptr;
}

} // namespace vm