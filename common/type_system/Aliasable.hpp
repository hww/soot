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

using namespace script;

// ============================================================================
// Aliasable
// ============================================================================

class Aliasable : public HeapObject {
public:
    using PropertyGetter = std::function<Object(Aliasable* self)>;
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
            const_cast<Aliasable*>(this)->ensure_aliases_defined();
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


class StaticBuffer : public Aliasable {
public:
    // Конструктор: привязываем буфер к конкретному типу из TypeSystem
    StaticBuffer(std::string type_name, int size, uint32_t origin = 0)
        : m_type_name(std::move(type_name)), m_origin(origin) {
        m_data.resize(size, 0); // Обнуляем память
        define_all_aliases();
    }

    // --- Интерфейс для C++ ---
    uint32_t origin() const { return m_origin; }
    size_t size() const { return m_data.size(); }
    uint8_t* data() { return m_data.data(); }
    const std::string& type_name() const { return m_type_name; }

    // Механизм записи (Endian-aware запись добавим позже)
    void write_u8(size_t offset, uint8_t value) {
        if (offset < m_data.size()) m_data[offset] = value;
    }

    // --- Реализация Aliasable для Лиспа ---
    void define_all_aliases() override;

    // Печать для REPL
    std::string print() const override {
        return "<static-buffer " + m_type_name + " at " + std::to_string(m_origin) + ">";
    }

private:
    std::string m_type_name;     // Ссылка на тип в TypeSystem
    uint32_t m_origin;           // Базовый адрес (например, #x2000)
    std::vector<uint8_t> m_data; // Сырые байты
    
    // В будущем: std::vector<Relocation> m_relocs;
};