#ifndef OPCODES_H
#define OPCODES_H

#include <cstdint>

// Упрощенная версия opcodes из твоей ВМ
enum class EOpcode : uint8_t {
    Return = 0,
    Move = 1,
    Call = 2,
    CallNat = 3,
    Branch = 4,
    BranchIf = 5,
    BranchIfNot = 6,
    
    // Integer operations
    AddInt = 7,
    SubInt = 8,
    MulInt = 9,
    DivInt = 10,
    LoadImediateInt = 11,
    
    // Floating point (пока не используем)
    AddFloat = 12,
    SubFloat = 13,
    
    // Comparison
    CmpEqual = 14,
    CmpGt = 15,
    CmpLt = 16,
    
    // Logical
    LogAnd = 17,
    LogOr = 18,
    LogNot = 19,
    
    // Lookup operations
    LookupInt = 20,
    LookupFloat = 21,
    LookupPointer = 22,
    
    // Load static
    LoadStaticInt = 23,
    LoadStaticFloat = 24,
    LoadStaticPointer = 25
};

// Константы из твоей ВМ
constexpr size_t ARGUMENT_REGISTERS_OFFSET = 24;
constexpr size_t LOCAL_REGISTERS_OFFSET = 0;
constexpr size_t DC_FRAME_MAX_REGISTERS_NUM = 34;

#endif