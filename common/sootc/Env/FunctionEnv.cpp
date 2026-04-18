#include "common/sootc/Env/FunctionEnv.hpp"
#include "common/sootc/IR/IR_Value.hpp"
#include "fmt/format.h"

namespace sootc {

// ============================================================================
// Construction
// ============================================================================

FunctionEnv::FunctionEnv(Env* parent, const std::string& name)
    : DeclareEnv(EnvKind::FUNCTION_ENV, parent), m_name(name) {}

// ============================================================================
// Env interface
// ============================================================================

std::string FunctionEnv::print() const {
    return fmt::format("FunctionEnv(name={}, params={}, locals={})", 
                       m_name, m_params.size(), m_iregs.size());
}

IR_Value* FunctionEnv::lookup(const std::string& name) {
    // 1. Параметры
    auto param_it = m_param_map.find(name);
    if (param_it != m_param_map.end()) {
        return param_it->second;
    }
    
    // 2. Локальные символы
    auto it = m_symbols_map.find(name);
    if (it != m_symbols_map.end()) {
        return it->second;
    }
    
    // 3. Родитель
    return m_parent ? m_parent->lookup(name) : nullptr;
}

void FunctionEnv::bind(const std::string& name, IR_Value* val) {
    if (m_symbols_map.find(name) == m_symbols_map.end()) {
        m_ordered_symbols.push_back(val);
    }
    m_symbols_map[name] = val;
}

std::string FunctionEnv::get_value_name(IR_Value* value) const {
    for (const auto& [name, val] : m_symbols_map) {
        if (val == value) return name;
    }
    for (const auto* param : m_params) {
        if (param == value) return "param";
    }
    return "<unknown>";
}

// ============================================================================
// Register management
// ============================================================================

IR_Reg* FunctionEnv::alloc_reg(Type* type) {
    auto reg = std::make_unique<IR_Reg>(type, m_next_reg++, false);
    IR_Reg* result = reg.get();
    m_iregs.push_back(std::move(reg));
    return result;
}

u8 FunctionEnv::get_reg_index(IR_Reg* reg) const {
    // Проверяем параметры
    for (size_t i = 0; i < m_params.size(); ++i) {
        if (m_params[i] == reg) {
            return static_cast<u8>(i);
        }
    }
    
    // Проверяем локальные регистры
    for (size_t i = 0; i < m_iregs.size(); ++i) {
        if (m_iregs[i].get() == reg) {
            return static_cast<u8>(m_params.size() + i);
        }
    }
    
    return 0; // Not found
}

// ============================================================================
// Parameters
// ============================================================================

void FunctionEnv::define_argument(const std::string& name, Type* type, int reg_index) {
    auto* reg = new IR_Reg(type, reg_index, true);
    m_params.push_back(reg);
    m_param_map[name] = reg;
    bind(name, reg);
}

IR_Reg* FunctionEnv::lookup_param(const std::string& name) const {
    auto it = m_param_map.find(name);
    return it != m_param_map.end() ? it->second : nullptr;
}

int FunctionEnv::get_max_param_index() const {
    int max_idx = -1;
    for (auto* reg : m_params) {
        if (static_cast<int>(reg->get_index()) > max_idx) {
            max_idx = reg->get_index();
        }
    }
    return max_idx;
}

} // namespace sootc