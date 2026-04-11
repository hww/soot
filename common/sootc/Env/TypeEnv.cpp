// common/sootc/Env/TypeEnv.cpp
#include "common/sootc/Env/TypeEnv.hpp"
#include "common/sootc/Env/MethodEnv.hpp"
#include "common/sootc/Env/StateEnv.hpp"
#include "common/sootc/Env/FileEnv.hpp"
#include "common/sootc/Env/GlobalEnv.hpp"  
#include "common/sootc/IR/IR_Value.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/type_system/Type.hpp"
#include "common/util/Log.hpp"
#include <stdexcept>
#include <unordered_set>

namespace sootc {

// ============================================================================
// Inheritance-aware lookup
// ============================================================================

MethodEnv* TypeEnv::find_method(int id) const {
    // 1. Ищем в текущем типе
    if (id >= 0 && id < (int)m_vtable_slots.size()) {
        if (m_vtable_slots[id] != nullptr) {
            return m_vtable_slots[id];
        }
    }
    
    // 2. Ищем в родительском типе
    if (m_type->has_parent()) {
        Type* parent_type = TypeSystem::instance().lookup_type(m_type->parent());
        
        // Получаем TypeEnv родителя через глобальное окружение
        // Используем const_cast для вызова не-const метода
        Env* global = const_cast<TypeEnv*>(this)->type_env();
        IR_Value* parent_val = global->lookup(parent_type->name());
        
        if (parent_val) {
            auto* parent_ir_type = dynamic_cast<IR_Type*>(parent_val);
            if (parent_ir_type) {
                TypeEnv* parent_env = parent_ir_type->get_env();
                return parent_env->find_method(id);
            }
        }
    }
    
    return nullptr;
}

MethodEnv* TypeEnv::find_method(const std::string& name) const {
    // 1. Ищем в текущем типе
    for (auto* m_env : m_vtable_slots) {
        if (m_env && m_env->get_name() == name) {
            return m_env;
        }
    }
    
    // 2. Ищем в родительском типе
    if (m_type->has_parent()) {
        Type* parent_type = TypeSystem::instance().lookup_type(m_type->parent());
        
        Env* global = global_env();
        IR_Value* parent_val = global->lookup(parent_type->name());
        
        if (parent_val) {
            auto* parent_ir_type = dynamic_cast<IR_Type*>(parent_val);
            if (parent_ir_type) {
                TypeEnv* parent_env = parent_ir_type->get_env();
                return parent_env->find_method(name);
            }
        }
    }
    
    return nullptr;
}

StateEnv* TypeEnv::find_state(const std::string& name) const {
    // 1. Ищем в текущем типе
    for (auto* s_env : m_states_list) {
        if (s_env->name() == name) {
            return s_env;
        }
    }
    
    // 2. Ищем в родительском типе
    if (m_type->has_parent()) {
        Type* parent_type = TypeSystem::instance().lookup_type(m_type->parent());
        
        Env* global = global_env();
        IR_Value* parent_val = global->lookup(parent_type->name());
        
        if (parent_val) {
            auto* parent_ir_type = dynamic_cast<IR_Type*>(parent_val);
            if (parent_ir_type) {
                TypeEnv* parent_env = parent_ir_type->get_env();
                return parent_env->find_state(name);
            }
        }
    }
    
    return nullptr;
}

// ============================================================================
// Utility methods
// ============================================================================

bool TypeEnv::has_method(const std::string& name) const {
    return find_method(name) != nullptr;
}

bool TypeEnv::has_method(int id) const {
    return find_method(id) != nullptr;
}

bool TypeEnv::has_state(const std::string& name) const {
    return find_state(name) != nullptr;
}

// ============================================================================
// Method creation helpers
// ============================================================================

MethodEnv* TypeEnv::create_method(int id, const std::string& name) {
    // Проверяем, что метод еще не существует
    if (find_method_local(name) != nullptr) {
        throw std::runtime_error(fmt::format(
            "Method '{}' already exists in type '{}'", name, m_name));
    }
    
    if (id < 0 || id >= (int)m_vtable_slots.size()) {
        throw std::runtime_error(fmt::format(
            "Method ID {} out of range for type '{}' (max: {})", 
            id, m_name, m_vtable_slots.size() - 1));
    }
    
    if (m_vtable_slots[id] != nullptr) {
        throw std::runtime_error(fmt::format(
            "VTable slot {} already occupied in type '{}'", id, m_name));
    }
    
    auto* m_env = new MethodEnv(id, name, this, m_type);
    m_vtable_slots[id] = m_env;
    return m_env;
}

MethodEnv* TypeEnv::get_or_create_method(int id, const std::string& name) {
    // Сначала ищем существующий (включая родительские)
    MethodEnv* existing = find_method(id);
    if (existing) {
        if (existing->get_name() != name) {
            lg::warn("Method ID {} in type '{}' has name '{}' but requested '{}'",
                     id, m_name, existing->get_name(), name);
        }
        return existing;
    }
    
    // Создаем новый в текущем типе
    return create_method(id, name);
}

// ============================================================================
// State creation helpers
// ============================================================================

StateEnv* TypeEnv::create_state(const std::string& name) {
    // Проверяем, что состояние еще не существует
    if (find_state_local(name) != nullptr) {
        throw std::runtime_error(fmt::format(
            "State '{}' already exists in type '{}'", name, m_name));
    }
    
    // Проверяем родительские типы
    if (find_state(name) != nullptr) {
        lg::warn("State '{}' shadows parent state in type '{}'", name, m_name);
    }
    
    auto* s_env = new StateEnv(name, this, m_type, this);
    m_states_list.push_back(s_env);
    return s_env;
}

StateEnv* TypeEnv::get_or_create_state(const std::string& name) {
    // Ищем существующий (включая родительские)
    StateEnv* existing = find_state(name);
    if (existing) {
        // Если состояние объявлено в родителе, но не переопределено
        if (existing->type_env() != this) {
            // Создаем локальную копию (переопределение)
            auto* local = create_state(name);
            
            // Копируем свойства из родительского
            local->set_type_spec(existing->type_spec());
            local->set_is_virtual(existing->is_virtual());
            
            return local;
        }
        return existing;
    }
    
    // Создаем новый
    return create_state(name);
}

// ============================================================================
// Debug and introspection
// ============================================================================

std::string TypeEnv::dump_vtable() const {
    std::string result = fmt::format("VTable for type '{}' ({} slots):\n", 
                                     m_name, m_vtable_slots.size());
    
    for (size_t i = 0; i < m_vtable_slots.size(); i++) {
        if (m_vtable_slots[i]) {
            auto* m_env = m_vtable_slots[i];
            result += fmt::format("  [{}] {} : {} (defined: {})\n",
                                  i, 
                                  m_env->get_name(),
                                  m_env->method_function_type.print(),
                                  m_env->is_defined());
        } else {
            // Ищем в родителях
            MethodEnv* parent_method = find_method(static_cast<int>(i));
            if (parent_method) {
                result += fmt::format("  [{}] {} : {} (inherited from {})\n",
                                      i,
                                      parent_method->get_name(),
                                      parent_method->method_function_type.print(),
                                      parent_method->type_env()->name());
            } else {
                result += fmt::format("  [{}] <empty>\n", i);
            }
        }
    }
    
    return result;
}

std::string TypeEnv::dump_states() const {
    std::string result = fmt::format("States for type '{}' ({} states):\n", 
                                     m_name, m_states_list.size());
    
    for (auto* s_env : m_states_list) {
        result += fmt::format("  {} : {} (virtual: {}, defined: {})\n",
                              s_env->name(),
                              s_env->type_spec().print(),
                              s_env->is_virtual(),
                              s_env->is_defined());
    }
    
    // Показываем также унаследованные состояния
    if (m_type->has_parent()) {
        Type* parent_type = TypeSystem::instance().lookup_type(m_type->parent());
        
        Env* global = global_env();
        IR_Value* parent_val = global->lookup(parent_type->name());
        
        if (parent_val) {
            auto* parent_ir_type = dynamic_cast<IR_Type*>(parent_val);
            if (parent_ir_type) {
                TypeEnv* parent_env = parent_ir_type->get_env();
                
                for (auto* s_env : parent_env->states()) {
                    // Проверяем, не переопределено ли локально
                    if (!find_state_local(s_env->name())) {
                        result += fmt::format("  {} : {} (inherited from {})\n",
                                              s_env->name(),
                                              s_env->type_spec().print(),
                                              parent_env->name());
                    }
                }
            }
        }
    }
    
    return result;
}

// ============================================================================
// Validation
// ============================================================================

bool TypeEnv::validate() const {
    bool valid = true;
    
    // Проверяем, что все слоты VTable заполнены или унаследованы
    for (size_t i = 0; i < m_vtable_slots.size(); i++) {
        if (m_vtable_slots[i] == nullptr) {
            // Проверяем, есть ли метод в родителях
            MethodEnv* parent_method = find_method(static_cast<int>(i));
            if (!parent_method) {
                lg::error("Type '{}': VTable slot {} is empty and not inherited", m_name, i);
                valid = false;
            }
        }
    }
    
    // Проверяем, что все объявленные методы имеют реализации
    for (auto* m_env : m_vtable_slots) {
        if (m_env && !m_env->is_defined()) {
            lg::error("Type '{}': Method '{}' is declared but not defined", 
                      m_name, m_env->get_name());
            valid = false;
        }
    }
    
    // Проверяем состояния
    for (auto* s_env : m_states_list) {
        if (!s_env->is_defined()) {
            lg::error("Type '{}': State '{}' is declared but not defined", 
                      m_name, s_env->name());
            valid = false;
        }
    }
    
    return valid;
}

// ============================================================================
// Inheritance helpers
// ============================================================================

void TypeEnv::inherit_from(TypeEnv* parent_env) {
    // Копируем методы из родителя (для слотов, которые не переопределены)
    for (size_t i = 0; i < m_vtable_slots.size() && i < parent_env->m_vtable_slots.size(); i++) {
        if (m_vtable_slots[i] == nullptr && parent_env->m_vtable_slots[i] != nullptr) {
            // Создаем прокси-метод, ссылающийся на родительский
            auto* parent_method = parent_env->m_vtable_slots[i];
            
            // ВАЖНО: Не копируем MethodEnv, а создаем ссылку
            // При генерации кода будет использоваться реализация родителя
            lg::debug("Type '{}' inherits method [{}] '{}' from '{}'",
                      m_name, i, parent_method->get_name(), parent_env->name());
        }
    }
    
    // Состояния автоматически наследуются через find_state()
    lg::debug("Type '{}' inherited {} methods and {} states from '{}'",
              m_name, 
              parent_env->method_count(),
              parent_env->state_count(),
              parent_env->name());
}

// ============================================================================
// Code generation helpers
// ============================================================================

std::vector<MethodEnv*> TypeEnv::get_all_methods_including_inherited() const {
    std::vector<MethodEnv*> result;
    result.resize(m_vtable_slots.size(), nullptr);
    
    for (size_t i = 0; i < m_vtable_slots.size(); i++) {
        result[i] = find_method(static_cast<int>(i));
    }
    
    return result;
}

std::vector<StateEnv*> TypeEnv::get_all_states_including_inherited() const {
    std::vector<StateEnv*> result;
    std::unordered_set<std::string> seen;
    
    // Сначала локальные
    for (auto* s_env : m_states_list) {
        result.push_back(s_env);
        seen.insert(s_env->name());
    }
    
    // Затем родительские (не переопределенные)
    if (m_type->has_parent()) {
        Type* parent_type = TypeSystem::instance().lookup_type(m_type->parent());
        
        Env* global = global_env();
        IR_Value* parent_val = global->lookup(parent_type->name());
        
        if (parent_val) {
            auto* parent_ir_type = dynamic_cast<IR_Type*>(parent_val);
            if (parent_ir_type) {
                TypeEnv* parent_env = parent_ir_type->get_env();
                
                for (auto* s_env : parent_env->states()) {
                    if (seen.find(s_env->name()) == seen.end()) {
                        result.push_back(s_env);
                        seen.insert(s_env->name());
                    }
                }
            }
        }
    }
    
    return result;
}

} // namespace sootc