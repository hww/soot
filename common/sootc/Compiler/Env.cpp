#include "Env.hpp"
#include <fmt/format.h>

namespace sootc {

// ============================================================================
// Env
// ============================================================================

Env::Env(EnvKind kind, Env* parent) 
    : m_kind(kind), m_parent(parent) {}

int Env::lookup_local(const std::string& name) {
    auto it = m_locals.find(name);
    if (it != m_locals.end()) {
        return it->second;
    }
    return m_parent ? m_parent->lookup_local(name) : -1;
}

void Env::add_local(const std::string& name, int reg_index) {
    m_locals[name] = reg_index;
}

std::string Env::print() const {
    return "Env";
}

FunctionEnv* Env::function_env() {
    Env* current = this;
    while (current) {
        if (current->kind() == EnvKind::FUNCTION_ENV) {
            return static_cast<FunctionEnv*>(current);
        }
        current = current->parent();
    }
    return nullptr;
}

GlobalEnv* Env::global_env() {
    Env* current = this;
    while (current) {
        if (current->kind() == EnvKind::GLOBAL_ENV) {
            return static_cast<GlobalEnv*>(current);
        }
        current = current->parent();
    }
    return nullptr;
}

LexicalEnv* Env::lexical_env() {
    Env* current = this;
    while (current) {
        if (current->kind() == EnvKind::LEXICAL_ENV) {
            return static_cast<LexicalEnv*>(current);
        }
        current = current->parent();
    }
    return nullptr;
}

// ============================================================================
// GlobalEnv
// ============================================================================

GlobalEnv::GlobalEnv() : Env(EnvKind::GLOBAL_ENV, nullptr) {}

void GlobalEnv::add_type(const std::string& name, Type* type) {
    m_types[name] = type;
}

Type* GlobalEnv::lookup_type(const std::string& name) {
    auto it = m_types.find(name);
    return it != m_types.end() ? it->second : nullptr;
}

void GlobalEnv::add_function(const std::string& name) {
    m_functions[name] = true;
}

bool GlobalEnv::has_function(const std::string& name) {
    return m_functions.find(name) != m_functions.end();
}

std::string GlobalEnv::print() const {
    return fmt::format("GlobalEnv(types={}, functions={})", 
                       m_types.size(), m_functions.size());
}

// ============================================================================
// FunctionEnv
// ============================================================================

FunctionEnv::FunctionEnv(Env* parent, const std::string& name, int arg_count)
    : Env(EnvKind::FUNCTION_ENV, parent),
      m_name(name),
      m_arg_count(arg_count),
      m_next_local_reg(arg_count) {}

int FunctionEnv::alloc_local_reg() {
    return m_next_local_reg++;
}

void FunctionEnv::add_arg(const std::string& name, int index) {
    if (index < 0 || index >= m_arg_count) {
        return;
    }
    int reg = get_arg_reg(index);
    add_local(name, reg);
    if ((int)m_arg_names.size() <= index) {
        m_arg_names.resize(index + 1);
    }
    m_arg_names[index] = name;
}

std::string FunctionEnv::print() const {
    return fmt::format("FunctionEnv({}, args={}, local_regs={})", 
                       m_name, m_arg_count, m_next_local_reg);
}

// ============================================================================
// LexicalEnv
// ============================================================================

LexicalEnv::LexicalEnv(Env* parent) 
    : Env(EnvKind::LEXICAL_ENV, parent) {}

std::string LexicalEnv::print() const {
    return fmt::format("LexicalEnv(vars={})", m_locals.size());
}

} // namespace sootc