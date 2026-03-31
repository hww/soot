#include "sootc/Compiler/Env.hpp"
#include "sootc/IR/IR_Node.hpp"

namespace sootc {

Env::Env(EnvKind kind, Env* parent) : m_kind(kind), m_parent(parent) {}

IR_Value* Env::lookup(const std::string& name) {
    auto it = m_symbols.find(name);
    if (it != m_symbols.end()) return it->second;
    return m_parent ? m_parent->lookup(name) : nullptr;
}

void Env::bind(const std::string& name, IR_Value* val) {
    m_symbols[name] = val;
}

void Env::emit(IR_Node* node) {
    if (m_parent) m_parent->emit(node);
}

FunctionEnv* Env::function_env() {
    if (m_kind == EnvKind::FUNCTION) return static_cast<FunctionEnv*>(this);
    return m_parent ? m_parent->function_env() : nullptr;
}

GlobalEnv* Env::global_env() {
    if (m_kind == EnvKind::GLOBAL) return static_cast<GlobalEnv*>(this);
    return m_parent ? m_parent->global_env() : nullptr;
}

GlobalEnv::GlobalEnv() : Env(EnvKind::GLOBAL, nullptr) {}

void GlobalEnv::add_type(const std::string& name, Type* type) { 
    m_types[name] = type; 
}

Type* GlobalEnv::lookup_type(const std::string& name) {
    auto it = m_types.find(name);
    return it != m_types.end() ? it->second : nullptr;
}

FunctionEnv::FunctionEnv(Env* parent, const std::string& name)
    : Env(EnvKind::FUNCTION, parent), m_name(name) {}

void FunctionEnv::define_argument(const std::string& name, Type* type, int reg_index) {
    auto* reg = new IR_Reg(type, reg_index, true);
    bind(name, reg);
    m_params.push_back(reg);
}

void FunctionEnv::emit(IR_Node* node) {
    m_nodes.push_back(node);
}

LexicalEnv::LexicalEnv(Env* parent) : Env(EnvKind::LEXICAL, parent) {}

} // namespace sootc