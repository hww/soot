// sootc/IR/StackVarAddrVal.hpp
#pragma once

#include "sootc/IR/IR_Value.hpp"
#include "fmt/format.h"

namespace sootc {

class StackVarAddrVal : public IR_Value {
public:
    StackVarAddrVal(Type* type, int slot, int slot_count)
        : IR_Value(type), m_slot(slot), m_slot_count(slot_count) {}
    
    int slot() const { return m_slot; }
    int slot_count() const { return m_slot_count; }
    
    std::string to_string() const override {
        return fmt::format("stack-{}", m_slot);
    }
    
    bool is_reg() const override { return false; }
    
    IR_Reg* to_reg(Env& env) override {
        // TODO: преобразовать адрес стека в регистр
        (void)env;
        return nullptr;
    }
    
private:
    int m_slot;
    int m_slot_count;
};

} // namespace sootc