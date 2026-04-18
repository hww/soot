#pragma once

#include "common/type_system/Type.hpp"

namespace sootc {

class LocalVariable {
    Type* m_type;
    u8 m_reg;
    
public:
    LocalVariable(Type* type, u8 reg) : m_type(type), m_reg(reg) {}
    
    Type* type() const { return m_type; }
    u8 reg() const { return m_reg; }
};

} // namespace sootc