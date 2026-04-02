#include "common/sootc/Env/FileEnv.hpp"
#include "common/sootc/Env/FunctionEnv.hpp"
#include "common/sootc/IR/StaticObject.hpp"
#include "common/sootc/IR/IR_Value.hpp"
#include "fmt/format.h"

namespace sootc {

// ============================================================================
// Construction
// ============================================================================

FileEnv::FileEnv(Env* parent, std::string name)
    : Env(EnvKind::FILE_ENV, parent), m_name(std::move(name)) {
    // FileEnv наследует от Env, который уже инициализировал кэш
}

// ============================================================================
// Env interface
// ============================================================================

std::string FileEnv::print() const {
    return fmt::format("FileEnv(name={}, functions={}, statics={})", 
                       m_name, m_functions.size(), m_statics.size());
}

IR_Value* FileEnv::lookup(const std::string& name) {
    // 1. Сначала ищем в локальной таблице символов файла
    auto it = m_symbols_map.find(name);
    if (it != m_symbols_map.end()) {
        return it->second;
    }
    
    // 2. Если не нашли, делегируем родителю (GlobalEnv)
    return m_parent ? m_parent->lookup(name) : nullptr;
}

void FileEnv::bind(const std::string& name, IR_Value* val) {
    // Добавляем в мап для быстрого поиска
    if (m_symbols_map.find(name) == m_symbols_map.end()) {
        m_ordered_symbols.push_back(val);
    }
    m_symbols_map[name] = val;
}

// ============================================================================
// Function management
// ============================================================================

void FileEnv::add_function(std::unique_ptr<FunctionEnv> fe) {
    m_functions.push_back(std::move(fe));
}

void FileEnv::add_top_level_function(std::unique_ptr<FunctionEnv> fe) {
    m_functions.push_back(std::move(fe));
    m_top_level_func = m_functions.back().get();
}

// ============================================================================
// Static data management
// ============================================================================

void FileEnv::add_static(std::unique_ptr<StaticObject> s) {
    m_statics.push_back(std::move(s));
}

// ============================================================================
// Utilities
// ============================================================================

bool FileEnv::is_empty() const {
    // Пустой файл = только top-level функция и в ней нет кода
    return m_functions.size() == 1 && 
           m_functions.front().get() == m_top_level_func &&
           m_top_level_func->code().empty();
}

void FileEnv::cleanup_after_codegen() {
    // Очищаем после генерации кода
    m_top_level_func = nullptr;
    m_functions.clear();
    m_statics.clear();
    m_vals.clear();
    m_symbols_map.clear();
    m_ordered_symbols.clear();
}

void FileEnv::debug_print_tl() const {
    if (m_top_level_func) {
        for (const auto& node : m_top_level_func->code()) {
            fmt::print("{}\n", node->print());
        }
    } else {
        fmt::print("No top level function.\n");
    }
}

} // namespace sootc