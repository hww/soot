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


enum class RelocType {
    ABS_ADDR,   // Абсолютный адрес (16/32 бита в зависимости от архитектуры)
    SYMBOL_CRC, // Запись CRC32/16 имени символа
    RELATIVE    // Относительный адрес (jump/call)
};

struct Relocation {
    size_t offset;
    RelocType type;
    std::string target_name; // Имя типа или символа
};

class StaticBuffer : public Aliasable {

public:
    enum class Endian { Little, Big };


    // Конструктор: привязываем буфер к конкретному типу из TypeSystem
    StaticBuffer(std::string type_name, int size, uint32_t origin = 0)
        : m_type_name(std::move(type_name)), m_origin(origin) {
        m_data.resize(size, 0); // Обнуляем память
        define_all_aliases();
    }
    
    void set_endian(Endian e) { m_endian = e; }
    // --- Интерфейс для C++ ---
    uint32_t origin() const { return m_origin; }
    size_t size() const { return m_data.size(); }
    uint8_t* data() { return m_data.data(); }
    const std::string& type_name() const { return m_type_name; }


    // --- Реализация Aliasable для Лиспа ---
    void define_all_aliases() override;

    // Печать для REPL
    std::string print() const override {
        return "<static-buffer " + m_type_name + " at " + std::to_string(m_origin) + ">";
    }

    // --- Реализация записи различных данных ---

    void write_u8(size_t offset, uint8_t value) {
        m_data[offset] = value;
    }

    void write_u16(size_t offset, uint16_t value) {
        if (m_endian == Endian::Little) 
            write_u16_le(offset, value);
        else 
            write_u16_be(offset, value);
    }

    void write_u32(size_t offset, uint16_t value) {
        if (m_endian == Endian::Little) 
            write_u32_le(offset, value);
        else 
            write_u32_be(offset, value);
    }

    /**
     * Записывает C-строку (массив байт) заданной длины.
     * Не добавляет нулевой терминатор автоматически.
     */
    void write_string(size_t offset, const char* str, int len) {
        if (!str || len <= 0) return;
        
        // Проверка, чтобы не выйти за пределы вектора
        if (offset + len > m_data.size()) {
            len = m_data.size() - offset; // Обрезаем, если строка не влезает
        }

        if (len > 0) {
            std::memcpy(m_data.data() + offset, str, len);
        }
    }

    /**
     * Записывает std::string целиком.
     * По умолчанию добавляет нулевой терминатор (null-terminated string).
     */
    void write_string(size_t offset, const std::string& str, bool null_terminated = true) {
        int len = static_cast<int>(str.length());
        write_string(offset, str.c_str(), len);
        
        if (null_terminated) {
            size_t term_offset = offset + len;
            if (term_offset < m_data.size()) {
                m_data[term_offset] = 0;
            }
        }
    }

    // --- Реализация реалокиции указателей ---

public:
    // API для добавления релокации
    void add_reloc(size_t offset, RelocType type, const std::string& target) {
        m_relocations.push_back({offset, type, target});
    }

    // Получить все релокации для линковщика
    const std::vector<Relocation>& get_relocations() const {
        return m_relocations;
    }

    // Очистить (если нужно пересобрать буфер)
    void clear_relocs() {
        m_relocations.clear();
    }

private:
    // --- 32-битные значения (4 байта) ---

    void write_u32_le(size_t offset, uint32_t value) {
        if (offset + 3 >= m_data.size()) return; // Защита от выхода за границы
        m_data[offset]     = (value & 0x000000FF);
        m_data[offset + 1] = (value & 0x0000FF00) >> 8;
        m_data[offset + 2] = (value & 0x00FF0000) >> 16;
        m_data[offset + 3] = (value & 0xFF000000) >> 24;
    }

    void write_u32_be(size_t offset, uint32_t value) {
        if (offset + 3 >= m_data.size()) return;
        m_data[offset]     = (value & 0xFF000000) >> 24;
        m_data[offset + 1] = (value & 0x00FF0000) >> 16;
        m_data[offset + 2] = (value & 0x0000FF00) >> 8;
        m_data[offset + 3] = (value & 0x000000FF);
    }

    // --- 16-битные значения (2 байта) ---

    void write_u16_le(size_t offset, uint16_t value) {
        if (offset + 1 >= m_data.size()) return;
        m_data[offset]     = (value & 0xFF);
        m_data[offset + 1] = (value >> 8) & 0xFF;
    }

    void write_u16_be(size_t offset, uint16_t value) {
        if (offset + 1 >= m_data.size()) return;
        m_data[offset]     = (value >> 8) & 0xFF;
        m_data[offset + 1] = (value & 0xFF);
    } 
private:
    std::string m_type_name;     // Ссылка на тип в TypeSystem
    uint32_t m_origin;           // Базовый адрес (например, #x2000)
    std::vector<uint8_t> m_data; // Сырые байты
    Endian m_endian = Endian::Little; // По умолчанию для Z80
    std::vector<Relocation> m_relocations;
};