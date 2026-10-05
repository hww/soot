#pragma once

#include "CommonTypes.hpp"

namespace carbon {

    /// @brief VM opcodes. Numeric values are stable and are baked into shipped .dc files.
    /// @details Do NOT reorder or insert values in the middle. Append new opcodes at the end.
    enum class Opcode : u8 {
        Return,               ///< return from function
        IAdd,                 ///< dest = op1 + op2 (integer)
        ISub,                 ///< dest = op1 - op2 (integer)
        IMul,                 ///< dest = op1 * op2 (integer)
        IDiv,                 ///< dest = op1 / op2 (integer)
        FAdd,                 ///< dest = op1 + op2 (f32)
        FSub,                 ///< dest = op1 - op2 (f32)
        FMul,                 ///< dest = op1 * op2 (f32)
        FDiv,                 ///< dest = op1 / op2 (f32)
        LoadStaticInt,        ///< dest = ST[op1] (i32)
        LoadStaticFloat,      ///< dest = ST[op1] (f32)
        LoadStaticPointer,    ///< dest = ST[op1] (pointer)
        LoadU16Imm,           ///< dest = op1 | (op2 << 8)
        LoadInt,              ///< dest = *(u32*)op1
        LoadFloat,            ///< dest = *(f32*)op1
        LoadPointer,          ///< dest = *(p64*)op1
        StoreInt,             ///< *(i32*)op1 = op2
        StoreFloat,           ///< *(f32*)op1 = op2
        StorePointer,         ///< *(p64*)op1 = op2
        LookupInt,            ///< dest = lookup(ST[op1]) as int
        LookupFloat,          ///< dest = lookup(ST[op1]) as float
        LookupPointer,        ///< dest = lookup(ST[op1]) as pointer
        MoveInt,              ///< dest = op1 (as int)
        MoveFloat,            ///< dest = op1 (as float)
        MovePointer,          ///< dest = op1 (as pointer)
        CastInteger,          ///< dest = (int)dest
        CastFloat,            ///< dest = (float)dest
        Call,                 ///< near call: dest = op1(args...), op2 = arg count
        CallFf,               ///< far call:  dest = op1(args...), op2 = arg count
        IEqual,               ///< dest = (op1 == op2) (integer)
        IGreaterThan,         ///< dest = (op1 >  op2) (integer)
        IGreaterThanEqual,    ///< dest = (op1 >= op2) (integer)
        ILessThan,            ///< dest = (op1 <  op2) (integer)
        ILessThanEqual,       ///< dest = (op1 <= op2) (integer)
        FEqual,               ///< dest = (op1 == op2) (f32)
        FGreaterThan,         ///< dest = (op1 >  op2) (f32)
        FGreaterThanEqual,    ///< dest = (op1 >= op2) (f32)
        FLessThan,            ///< dest = (op1 <  op2) (f32)
        FLessThanEqual,       ///< dest = (op1 <= op2) (f32)
        IMod,                 ///< dest = op1 % op2 (integer)
        FMod,                 ///< dest = fmod(op1, op2) (f32)
        IAbs,                 ///< dest = abs(op1) (integer)
        FAbs,                 ///< dest = fabs(op1) (f32)
        GoTo,                 ///< unconditional jump (label form)
        Label,                ///< label marker
        Branch,               ///< unconditional branch; target = op1 | (op2 << 8)
        BranchIf,             ///< branch if op1 != 0
        BranchIfNot,          ///< branch if op1 == 0
        OpLogNot,             ///< dest = !op1
        OpBitAnd,             ///< dest = op1 & op2
        OpBitNot,             ///< dest = ~op1
        OpBitOr,              ///< dest = op1 | op2
        OpBitXor,             ///< dest = op1 ^ op2
        OpBitNor,             ///< dest = ~(op1 | op2)
        OpLogAnd,             ///< dest = op1 && op2
        OpLogOr,              ///< dest = op1 || op2
        INeg,                 ///< dest = -op1 (integer)
        FNeg,                 ///< dest = -op1 (f32)
        LoadParamCnt,         ///< dest = argument count of the current call
        IAddImm,              ///< dest = op1 + imm16
        ISubImm,              ///< dest = op1 - imm16
        IMulImm,              ///< dest = op1 * imm16
        IDivImm,              ///< dest = op1 / imm16
        LoadStaticI32Imm,     ///< dest = ST[imm16] (i32)
        LoadStaticFloatImm,   ///< dest = ST[imm16] (f32)
        LoadStaticPointerImm, ///< dest = ST[imm16] (pointer)
        IntAsh,               ///< dest = op1 << op2 (op2 < 0 -> arithmetic right shift)
        Move,                 ///< dest = op1 (copies value + type + pointer offset)
        LoadStaticU32Imm,     ///< dest = ST[imm16] (u32)
        LoadStaticI8Imm,      ///< dest = ST[imm16] (i8)
        LoadStaticU8Imm,      ///< dest = ST[imm16] (u8)
        LoadStaticI16Imm,     ///< dest = ST[imm16] (i16)
        LoadStaticU16Imm,     ///< dest = ST[imm16] (u16)
        LoadStaticI64Imm,     ///< dest = ST[imm16] (i64)
        LoadStaticU64Imm,     ///< dest = ST[imm16] (u64 / sid64)
        LoadI8,               ///< dest = *(i8*)op1
        LoadU8,               ///< dest = *(u8*)op1
        LoadI16,              ///< dest = *(i16*)op1
        LoadU16,              ///< dest = *(u16*)op1
        LoadI32,              ///< dest = *(i32*)op1
        LoadU32,              ///< dest = *(u32*)op1
        LoadI64,              ///< dest = *(i64*)op1
        LoadU64,              ///< dest = *(u64*)op1
        StoreI8,              ///< *(i8*)op1 = op2
        StoreU8,              ///< *(u8*)op1 = op2
        StoreI16,             ///< *(i16*)op1 = op2
        StoreU16,             ///< *(u16*)op1 = op2
        StoreI32,             ///< *(i32*)op1 = op2
        StoreU32,             ///< *(u32*)op1 = op2
        StoreI64,             ///< *(i64*)op1 = op2
        StoreU64,             ///< *(u64*)op1 = op2
        INotEqual,            ///< dest = (op1 != op2) (integer)
        FNotEqual,            ///< dest = (op1 != op2) (f32)
        StoreArray,           ///< copy 16 bytes: *(xmmword*)op1 = *(xmmword*)op2
        AssertPointer,        ///< assert(op1 != nullptr)
        BreakFlag,            ///< break flag (unused by VM)
        Breakpoint,           ///< debugger breakpoint
    };

} // namespace carbon