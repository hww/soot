#pragma once

#include "common/sootc/Env/Env.hpp"
#include "common/sootc/IR/StaticObject.hpp"  
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace sootc {

class FunctionEnv;
class StaticObject;
class IR_Value;

/*!
 * An Env for an entire file (or input to the REPL)
 */
class FileEnv : public Env {
public:
    FileEnv(Env* parent, std::string name);
    ~FileEnv() = default;

    // ========================================================================
    // Env interface
    // ========================================================================
    std::string print() const override;
    IR_Value* lookup(const std::string& name) override;
    void bind(const std::string& name, IR_Value* val) override;

    const std::vector<IR_Value*>& symbols() const override { 
        return m_ordered_symbols; 
    }

    std::string get_value_name(IR_Value* value) const override;

    // ========================================================================
    // Import management
    // ========================================================================
    const std::vector<FileEnv*>& imported_envs() const { return m_imports; }
    
    void add_import(FileEnv* imported) {
        // Избегаем дубликатов
        if (!has_import(imported->name())) {
            m_imports.push_back(imported);
        }
    }
    
    const std::vector<FileEnv*>& imports() const { return m_imports; }
    
    bool has_import(const std::string& name) const {
        for (auto* imp : m_imports) {
            if (imp->name() == name) return true;
        }
        return false;
    }

    // ========================================================================
    // Function management
    // ========================================================================
    void add_function(std::unique_ptr<FunctionEnv> fe);
    void add_top_level_function(std::unique_ptr<FunctionEnv> fe);
    
    const std::vector<std::unique_ptr<FunctionEnv>>& functions() const { 
        return m_functions; 
    }
    
    const FunctionEnv* top_level_function() const { return m_top_level_func; }
    FunctionEnv* top_level_function() { return m_top_level_func; }
    
    std::string get_anon_function_name() {
        return "anon-function-" + std::to_string(m_anon_func_counter++);
    }

    // ========================================================================
    // Static data management
    // ========================================================================
    void add_static(std::unique_ptr<StaticObject> s);
    const std::vector<std::unique_ptr<StaticObject>>& statics() const { 
        return m_statics; 
    }

    // ========================================================================
    // Segment management (для GOAL совместимости)
    // ========================================================================
    int default_segment() const { return m_default_segment; }
    void set_nondebug_file() { m_default_segment = MAIN_SEGMENT; }
    void set_debug_file() { m_default_segment = DEBUG_SEGMENT; }
    bool is_debug_file() const { return default_segment() == DEBUG_SEGMENT; }

    // ========================================================================
    // Utilities
    // ========================================================================
    const std::string& name() const { return m_name; }
    bool is_empty() const;
    void cleanup_after_codegen();
    void debug_print_tl() const;

    // ========================================================================
    // Memory management for temporary values
    // ========================================================================
    template <typename T, class... Args>
    T* alloc_val(Args&&... args) {
        auto new_obj = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = new_obj.get();
        m_vals.push_back(std::move(new_obj));
        return ptr;
    }

    // ========================================================================
    // Required files tracking (для зависимостей)
    // ========================================================================
    std::unordered_set<std::string> m_required_files;
    std::unordered_set<std::string> m_missing_required_files;

    // ========================================================================
    // Symbol iteration
    // ========================================================================
    const std::unordered_map<std::string, IR_Value*>& symbols_map() const {
        return m_symbols_map;
    }

    // ========================================================================
    // Function lookup (отдельно от IR_Value)
    // ========================================================================
    FunctionEnv* find_function(const std::string& name) const;
    
protected:

    std::string m_name;
    std::vector<std::unique_ptr<FunctionEnv>> m_functions;
    std::vector<std::unique_ptr<StaticObject>> m_statics;
    std::vector<std::unique_ptr<IR_Value>> m_vals;  // для временных значений
    int m_anon_func_counter = 0;
    int m_default_segment = MAIN_SEGMENT;
    
    // Top-level function (точка входа файла)
    FunctionEnv* m_top_level_func = nullptr;
    
    // Таблица символов файла (только для IR_Value)
    std::unordered_map<std::string, IR_Value*> m_symbols_map;
    std::vector<IR_Value*> m_ordered_symbols;
    
    // Файлы, которые были (require ...) в этом файле
    std::vector<FileEnv*> m_imports;

    // Константы сегментов
    static constexpr int MAIN_SEGMENT = 0;
    static constexpr int DEBUG_SEGMENT = 1;
};

} // namespace sootc