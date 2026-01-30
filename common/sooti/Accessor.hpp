#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <optional>
#include <functional>
#include "common/sooti/Object.hpp" 

namespace script {

// ============================================================================
// Accessor
// ============================================================================

class Accessor : public HeapObject {
public:
    using PropertyGetter = std::function<Object(Accessor* self)>;
    using PropertyMap = std::unordered_map<std::string, PropertyGetter>;

protected:
    PropertyMap m_props; 
    bool m_aliases_defined = false;

    /**
     * Регистрация "виртуальных" свойств (метаданных)
     */
    void define_alias(std::string name, PropertyGetter func) {
        m_props[std::move(name)] = std::move(func);
    }

    /**
     * Метод для внутреннего поиска по мапе
     */
    Object find_property_in_map(const Object& key) {
        std::string name = key.to_std_string();
        const auto& props = get_property_map();
        
        auto it = props.find(name);
        if (it != props.end()) {
            return it->second(this);
        }
        
        // Если ничего не нашли, возвращаем undefined/nil, а не пустой список
        return Object::make_undefined(); 
    }

public:
    /**
     * Получить список всех определений
     */
    const PropertyMap& get_property_map() const {
        if (!m_aliases_defined) {
            const_cast<Accessor*>(this)->ensure_aliases_defined();
        }
        return m_props;
    }

    /**
     * Проверка определения define_alias()
     */ 
    void ensure_aliases_defined() {
        if (m_aliases_defined) return;
        define_all_aliases();
        m_aliases_defined = true;
    }

    /**
     * Наследники переопределяют это, чтобы вызвать define_alias()
     */ 
    virtual void define_all_aliases() {}

    /**
     * ГЛАВНЫЙ МЕТОД НАВИГАЦИИ (->)
     * Вызывается интерпретатором для объектов типа NATIVE_REF.
     */
    Object make_step_alias(const Object& key) override;
};

}