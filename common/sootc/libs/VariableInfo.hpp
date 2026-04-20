// VariableInfo.hpp
#pragma once

#include "common/type_system/Type.hpp"
#include <string>

namespace sootc {

class VariableInfo {
    std::string m_name;
    Type* m_type;
    u8 m_reg;
    bool m_is_parameter;
    
public:
    VariableInfo(const std::string& name, Type* type, u8 reg, bool is_param = false)
        : m_name(name), m_type(type), m_reg(reg), m_is_parameter(is_param) {}
    
    const std::string& name() const { return m_name; }
    Type* type() const { return m_type; }
    u8 reg() const { return m_reg; }
    bool is_parameter() const { return m_is_parameter; }
};

} // namespace sootc