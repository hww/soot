#include "common/sootc/Env/FileEnv.hpp"
#include "common/sootc/Env/FunctionEnv.hpp"
#include "common/sootc/Env/GlobalEnv.hpp"
#include "common/sootc/IR/StaticObject.hpp"
#include "common/sootc/IR/IR_Value.hpp"
#include "fmt/format.h"

namespace sootc {

// ============================================================================
// Construction
// ============================================================================

FileEnv::FileEnv(Env* parent, std::string name)
    : Env(EnvKind::FILE_ENV, parent), m_name(std::move(name)) {
}

// ============================================================================
// Env interface
// ============================================================================

std::string FileEnv::print() const {
    return fmt::format("FileEnv(name={}, functions={}, statics={}, symbols={})", 
                       m_name, m_functions.size(), m_statics.size(), m_symbols_map.size());
}
   
IR_Value* FileEnv::lookup(const std::string& name) {
    // 1. Свои символы
    auto it = m_symbols_map.find(name);
    if (it != m_symbols_map.end()) {
        return it->second;
    }
    
    // 2. Импортированные модули
    for (auto* imp : m_imports) {
        auto* val = imp->lookup(name);
        if (val) {
            return val;
        }
    }
    
    // 3. Родительское окружение (обычно GlobalEnv)
    if (m_parent) {
        return m_parent->lookup(name);
    }
    
    return nullptr;
}

void FileEnv::bind(const std::string& name, IR_Value* val) {
    // Добавляем в мап для быстрого поиска
    if (m_symbols_map.find(name) == m_symbols_map.end()) {
        m_ordered_symbols.push_back(val);
    }
    m_symbols_map[name] = val;
}

std::string FileEnv::get_value_name(IR_Value* value) const {
    if (!value) return "<null>";
    
    for (const auto& [name, val] : m_symbols_map) {
        if (val == value) return name;
    }
    
    // Ищем в родителе
    if (m_parent) {
        return m_parent->get_value_name(value);
    }
    
    return "<unknown>";
}

// ============================================================================
// Function management
// ============================================================================

void FileEnv::add_function(std::unique_ptr<FunctionEnv> fe) {
    // FunctionEnv не является IR_Value, поэтому не биндим его как значение
    // Вместо этого функция будет доступна через специальный механизм
    m_functions.push_back(std::move(fe));
}

void FileEnv::add_top_level_function(std::unique_ptr<FunctionEnv> fe) {
    m_functions.push_back(std::move(fe));
    m_top_level_func = m_functions.back().get();
}

FunctionEnv* FileEnv::find_function(const std::string& name) const {
    for (const auto& func : m_functions) {
        if (func->get_name() == name) {
            return func.get();
        }
    }
    return nullptr;
}

// ============================================================================
// Static data management
// ============================================================================

void FileEnv::add_static(std::unique_ptr<StaticObject> s) {
    // Используем name() вместо get_name()
    m_statics.push_back(std::move(s));
}

// ============================================================================
// Utilities
// ============================================================================

bool FileEnv::is_empty() const {
    // Пустой файл = только top-level функция и в ней нет кода
    if (m_functions.size() == 1 && m_top_level_func != nullptr) {
        return m_top_level_func->code().empty();
    }
    return m_functions.empty() && m_statics.empty();
}

void FileEnv::cleanup_after_codegen() {
    // Очищаем после генерации кода (но сохраняем символы для возможных ссылок)
    m_top_level_func = nullptr;
    m_functions.clear();
    m_statics.clear();
    m_vals.clear();
    // Символы и импорты сохраняем для информации о зависимостях
}

void FileEnv::debug_print_tl() const {
    if (m_top_level_func) {
        fmt::print("=== Top-level function for '{}' ===\n", m_name);
        for (size_t i = 0; i < m_top_level_func->code().size(); ++i) {
            fmt::print("  [{}] {}\n", i, m_top_level_func->code()[i]->print());
        }
        fmt::print("=== End top-level ===\n");
    } else {
        fmt::print("No top level function in '{}'.\n", m_name);
    }
}

} // namespace sootc