#pragma once

#include "common/sootc/Env/Env.hpp"
#include "common/sootc/Env/MethodEnv.hpp"

namespace sootc {

class MethodEnv;
class TypeEnv;

// State — это контейнер, не функция
class StateEnv : public Env {  // ← наследуем Env, НЕ FunctionEnv
public:
    StateEnv(const std::string& name, Env* parent, Type* type, TypeEnv* type_env = nullptr)
        : Env(EnvKind::STATE_ENV, parent), m_name(name), m_type(type), m_type_env(type_env),
          m_source_form(), m_is_virtual(false),  
          m_enter_method(), m_exit_method(), m_code_method(), 
          m_post_method(), m_trans_method(), m_event_method() {}
    
    const std::string& name() const { return m_name; }
    const Type* type() const { return m_type; }
    Type* type() { return m_type; }
    TypeEnv* type_env() const { return m_type_env; }
    
    void set_is_virtual(bool is_virtual) { m_is_virtual = is_virtual; }
    bool is_virtual() { return m_is_virtual; }

    // Обработчики состояния — это методы (функции)
    void set_enter_method(MethodEnv* method) { m_enter_method = method; }
    void set_exit_method(MethodEnv* method) { m_exit_method = method; }
    void set_code_method(MethodEnv* method) { m_code_method = method; }
    void set_post_method(MethodEnv* method) { m_post_method = method; }
    void set_trans_method(MethodEnv* method) { m_trans_method = method; }
    void set_event_method(MethodEnv* method) { m_event_method = method; }
    
    MethodEnv* enter_method() const { return m_enter_method; }
    MethodEnv* exit_method() const { return m_exit_method; }
    MethodEnv* code_method() const { return m_code_method; }
    MethodEnv* post_method() const { return m_post_method; }
    MethodEnv* trans_method() const { return m_trans_method; }
    MethodEnv* event_method() const { return m_event_method; }
    
    void set_type_env(TypeEnv* env) { m_type_env = env; }

    std::string print() const override {
        return fmt::format("StateEnv(name={}, type={})", 
                           m_name, m_type ? m_type->name() : "unknown");
    }

    void set_source_form(const soot::Object& form) { m_source_form = form; }
    soot::Object source_form() const { return m_source_form; }    

    void set_defined(bool defined) { m_is_defined = defined; }
    bool is_defined() const { return m_is_defined; }
    
    void set_type_spec(const TypeSpec& spec) { m_type_spec = spec; }
    const TypeSpec& type_spec() const { return m_type_spec; }

private:
    std::string m_name;
    TypeSpec m_type_spec; // аргументы функии code
    Type* m_type;
    TypeEnv* m_type_env = nullptr;
    soot::Object m_source_form;

    bool m_is_virtual;

    // Ссылки на методы-обработчики (функции)
    MethodEnv* m_enter_method = nullptr; 
    MethodEnv* m_exit_method = nullptr; 
    MethodEnv* m_code_method = nullptr; 
    MethodEnv* m_post_method = nullptr; 
    MethodEnv* m_trans_method = nullptr; 
    MethodEnv* m_event_method = nullptr;     

    bool m_is_defined = false;
};

} // namespace sootc