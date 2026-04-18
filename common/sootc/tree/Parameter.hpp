// Parameter.hpp
#pragma once

#include <string>
#include "common/type_system/Type.hpp"

namespace sootc {

class Parameter {
    std::string m_name;
    Type* m_type;
    u8 m_reg;  // номер регистра
    
public:
    Parameter(const std::string& name, Type* type, u8 reg)
        : m_name(name), m_type(type), m_reg(reg) {}
    
    const std::string& name() const { return m_name; }
    Type* type() const { return m_type; }
    u8 reg() const { return m_reg; }
};

} // namespace sootc