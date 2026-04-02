#pragma once

#include "common/sootc/Env/Env.hpp"
#include "common/sootc/Env/MethodEnv.hpp" 
#include "common/sootc/Env/StateEnv.hpp"  
#include "common/type_system/Type.hpp"    
#include "type_system/TypeSystem.hpp"
#include <vector>

namespace sootc {


class TypeEnv : public Env {
public:
    TypeEnv(const std::string& name, Type* type, Env* parent) 
        : Env(EnvKind::TYPE_ENV, parent), m_name(name), m_type(type) {
        // Резервируем слоты под VTable сразу
        m_vtable_slots.resize(m_type->get_methods_count(), nullptr);
    }

    // ========================================================================
    // Env interface
    // ========================================================================
    std::string print() const override {
        return fmt::format("TypeEnv(name={}, methods={}, states={})", 
                           m_name, m_vtable_slots.size(), m_states_list.size());
    }
    
    // Для IR_Value (данные, переменные)
    IR_Value* lookup(const std::string& name) override {
        return m_parent ? m_parent->lookup(name) : nullptr;
    }
    
    // Для методов и состояний (отдельный поиск)
    MethodEnv* lookup_method(const std::string& name) const {
        for (auto* m_env : m_vtable_slots) {
            if (m_env && m_env->name() == name) {
                return m_env;
            }
        }
        return nullptr;
    }
    
    StateEnv* lookup_state(const std::string& name) const {
        for (auto* s_env : m_states_list) {
            if (s_env->name() == name) {
                return s_env;
            }
        }
        return nullptr;
    }

    
    void bind(const std::string& name, IR_Value* val) override {
        // 1. Стандартный bind
        Env::bind(name, val);

        // 2. Специфичная логика для TypeEnv
        if (auto* m_env = dynamic_cast<MethodEnv*>(val)) {
            MethodInfo m_info;
            if (m_type->get_my_method(name, &m_info)) {
                m_vtable_slots[m_info.id] = m_env;
                m_env->set_type_env(this);  // обратная ссылка
            }
        } else if (auto* s_env = dynamic_cast<StateEnv*>(val)) {
            m_states_list.push_back(s_env);
            s_env->set_type_env(this);  // обратная ссылка
        }
    }

    // ========================================================================
    // VTable management
    // ========================================================================
    const std::vector<MethodEnv*>& vtable() const { return m_vtable_slots; }
    size_t method_count() const { return m_vtable_slots.size(); }
    
    // Для ID-based поиска (vtable)
    MethodEnv* get_method(int id) const {
        if (id >= 0 && id < (int)m_vtable_slots.size()) {
            return m_vtable_slots[id];
        }
        return nullptr;
    }
    
    MethodEnv* find_method_local(const std::string& name) const {
        for (auto* m_env : m_vtable_slots) {
            if (m_env && m_env->name() == name) {
                return m_env;
            }
        }
        return nullptr;
    }

    // ========================================================================
    // State management
    // ========================================================================
    const std::vector<StateEnv*>& states() const { return m_states_list; }
    size_t state_count() const { return m_states_list.size(); }
    
    StateEnv* find_state_local(const std::string& name) const {
        for (auto* s_env : m_states_list) {
            if (s_env->name() == name) {
                return s_env;
            }
        }
        return nullptr;
    }

    // ========================================================================
    // Inheritance-aware lookup
    // ========================================================================
    MethodEnv* find_method(int id) const;
    MethodEnv* find_method(const std::string& name) const;
    StateEnv* find_state(const std::string& name) const;

    // ========================================================================
    // Getters
    // ========================================================================
    const std::string& name() const { return m_name; }
    Type* get_type() const { return m_type; }
    Type* get_parent_type() const { return TypeSystem::instance().lookup_type(m_type->get_parent()); }
    
    // Для отложенной компиляции — установка смещения
    void set_file_offset(u64 offset) { m_file_offset = offset; }
    u64 get_file_offset() const { return m_file_offset; }

private:
    std::string m_name;
    Type* m_type;
    u64 m_file_offset = 0;
    
    std::vector<MethodEnv*> m_vtable_slots;   // Индексировано по MethodInfo.id
    std::vector<StateEnv*> m_states_list;     // Список состояний
};

} // namespace sootc