#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "common/type_system/TypeSystem.hpp"
#include "sootc/IR/IR_Value.hpp"

namespace sootc {

class Env;
class FunctionEnv;
class GlobalEnv;
class LexicalEnv;

enum class EnvKind {
    GLOBAL_ENV,
    FUNCTION_ENV,
    LEXICAL_ENV
};

// ============================================================================
// Base Env
// ============================================================================

class Env {
public:
    explicit Env(EnvKind kind, Env* parent = nullptr);
    virtual ~Env() = default;
    
    virtual int lookup_local(const std::string& name);
    virtual void add_local(const std::string& name, int reg_index);
    
    Env* parent() { return m_parent; }
    EnvKind kind() const { return m_kind; }
    
    FunctionEnv* function_env();
    GlobalEnv* global_env();
    LexicalEnv* lexical_env();
    
    virtual std::string print() const;
    
protected:
    EnvKind m_kind;
    Env* m_parent;
    std::unordered_map<std::string, int> m_locals;
};

// ============================================================================
// GlobalEnv
// ============================================================================

class GlobalEnv : public Env {
public:
    GlobalEnv();
    ~GlobalEnv() override = default;
    
    void add_type(const std::string& name, Type* type);
    Type* lookup_type(const std::string& name);
    
    void add_function(const std::string& name);
    bool has_function(const std::string& name);
    
    std::string print() const override;
    
private:
    std::unordered_map<std::string, Type*> m_types;
    std::unordered_map<std::string, bool> m_functions;
};

// ============================================================================
// FunctionEnv
// ============================================================================

class FunctionEnv : public Env {
public:
    FunctionEnv(Env* parent, const std::string& name, int arg_count);
    ~FunctionEnv() override = default;
    
    // Выделение нового регистра для локальной переменной
    int alloc_local_reg();
    
    // Регистр аргумента по индексу
    static int get_arg_reg(int index) {
        return ARG_REGISTERS_OFFSET + index;  // аргументы в регистрах 0, 1, 2...
    }
    
    // Добавление аргумента
    void add_arg(const std::string& name, int index);
    
    // Геттеры
    const std::string& name() const { return m_name; }
    int arg_count() const { return m_arg_count; }
    int next_local_reg() const { return m_next_local_reg; }
    
    std::string print() const override;
    
private:
    std::string m_name;
    int m_arg_count;
    int m_next_local_reg;  // следующий свободный регистр для локальных переменных
    std::vector<std::string> m_arg_names;
};

// ============================================================================
// LexicalEnv
// ============================================================================

class LexicalEnv : public Env {
public:
    explicit LexicalEnv(Env* parent);
    ~LexicalEnv() override = default;
    
    std::string print() const override;
};

} // namespace sootc