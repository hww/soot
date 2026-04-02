#pragma once

#include "Env.hpp"

namespace sootc {

/*!
 * An Env for managing labels aka (let loop () ... (go loop)).
 */
class LabelEnv : public Env {
public:
    explicit LabelEnv(Env* parent) : Env(EnvKind::LABEL_ENV, parent) {}
    
    std::string print() const override { 
        return fmt::format("LabelEnv(labels={})", m_labels.size()); 
    }
    
    // Возвращает карту меток для модификации
    std::unordered_map<std::string, Label>& get_label_map() override { 
        return m_labels; 
    }
    
    // Поиск блока по имени (может делегировать родителю)
    BlockEnv* find_block(const std::string& name) override {
        // Сначала ищем в текущем LabelEnv
        // Потом в родителе
        return m_parent ? m_parent->find_block(name) : nullptr;
    }
    
    // Добавление метки
    void add_label(const std::string& name, u64 offset = 0) {
        Label label;
        label.name = name;
        label.offset = offset;
        m_labels[name] = label;
    }
    
    // Поиск метки
    Label* find_label(const std::string& name) {
        auto it = m_labels.find(name);
        if (it != m_labels.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
protected:
    std::unordered_map<std::string, Label> m_labels;
};

} // namespace sootc