#include "common/sootc/Env/FileEnv.hpp"
#include "common/sootc/Env/FunctionEnv.hpp"
#include "common/sootc/IR/IR_FunctionValue.hpp"
#include "fmt/format.h"

namespace sootc {

// ============================================================================
// Constructor
// ============================================================================

FileEnv::FileEnv(Env* parent, const std::string& name)
    : Env(EnvKind::FILE_ENV, parent), m_name(name) {}

// ============================================================================
// Env interface
// ============================================================================

std::string FileEnv::print() const {
    return fmt::format("FileEnv(name={}, symbols={}, imports={})", 
                       m_name, m_symbols.size(), m_imports.size());
}

IR_Value* FileEnv::lookup(const std::string& name) {
    // 1. Свои символы
    auto it = m_symbols.find(name);
    if (it != m_symbols.end()) {
        return it->second;
    }
    
    // 2. Импортированные модули
    for (auto* imp : m_imports) {
        auto* val = imp->lookup(name);
        if (val) {
            return val;
        }
    }
    
    // 3. Родитель
    if (m_parent) {
        return m_parent->lookup(name);
    }
    
    return nullptr;
}

void FileEnv::bind(const std::string& name, IR_Value* val) {
    if (m_symbols.find(name) == m_symbols.end()) {
        m_ordered_symbols.push_back(val);
    }
    m_symbols[name] = val;
}

std::string FileEnv::get_value_name(IR_Value* value) const {
    for (const auto& [name, val] : m_symbols) {
        if (val == value) return name;
    }
    if (m_parent) {
        return m_parent->get_value_name(value);
    }
    return "<unknown>";
}

const std::vector<IR_Value*>& FileEnv::symbols() const { 
    return m_ordered_symbols; 
}

// ============================================================================
// Imports
// ============================================================================

void FileEnv::add_import(FileEnv* imported) {
    if (!has_import(imported->name())) {
        m_imports.push_back(imported);
    }
}

const std::vector<FileEnv*>& FileEnv::imports() const { 
    return m_imports; 
}

bool FileEnv::has_import(const std::string& name) const {
    for (auto* imp : m_imports) {
        if (imp->name() == name) return true;
    }
    return false;
}

// ============================================================================
// Function management
// ============================================================================

IR_FunctionValue* FileEnv::find_function(const std::string& name) const {
    for (auto* fn : m_functions) {
        if (fn->get_name() == name) {
            return fn;
        }
    }
    return nullptr;
}

void FileEnv::add_function(IR_FunctionValue* fn) {
    m_functions.push_back(fn);
}

// ============================================================================
// Old style functions (для совместимости)
// ============================================================================

void FileEnv::add_function_old(std::unique_ptr<FunctionEnv> fe) {
    m_functions_old.push_back(std::move(fe));
}

} // namespace sootc