#pragma once

#include <memory>
// Нам нужны полные определения Object и Arguments для сигнатур методов
#include "common/sooti/Object.hpp" 

class Type;
class TypeSystem;
class StructureType;

namespace script {

class Interpreter;
class EnvironmentObject;

enum class RelocType {
    ABS_ADDR,           // Абсолютный адрес (16/32 бита в зависимости от архитектуры)
    SYMBOL_CRC,         // Запись CRC32/16 имени символа
    RELATIVE,           // Относительный адрес (jump/call)
    SYMBOL_TABLE_REF
};

struct Relocation {
    size_t offset;
    RelocType type;
    std::string target_name; // Имя типа или символа
};

class StaticBuffer;

class StaticSymbolTable {
private:
    struct SymbolEntry {
        uint32_t crc32;
        uint32_t string_offset;  // offset в string pool
        std::string name;
    };
    
    std::vector<SymbolEntry> m_symbols;
    std::string m_string_pool;
    std::unordered_map<uint32_t, size_t> m_crc_to_index; // для быстрого поиска
    
public:
    StaticSymbolTable() = default;
    
    // Добавить символ, возвращает индекс
    size_t add_symbol(const std::string& name) {
        uint32_t crc = util::compute_crc32(name);
        
        // Проверяем, не добавлен ли уже
        auto it = m_crc_to_index.find(crc);
        if (it != m_crc_to_index.end()) {
            return it->second;
        }
        
        // Добавляем в string pool
        uint32_t string_offset = static_cast<uint32_t>(m_string_pool.size());
        m_string_pool.append(name);
        m_string_pool.push_back('\0');
        
        // Добавляем запись
        SymbolEntry entry{crc, string_offset, name};
        size_t index = m_symbols.size();
        m_symbols.push_back(entry);
        m_crc_to_index[crc] = index;
        
        return index;
    }
    
    // Получить CRC32 по индексу
    uint32_t get_crc32(size_t index) const {
        if (index >= m_symbols.size()) return 0;
        return m_symbols[index].crc32;
    }
    
    // Получить смещение строки
    uint32_t get_string_offset(size_t index) const {
        if (index >= m_symbols.size()) return 0;
        return m_symbols[index].string_offset;
    }
    
    // Найти по CRC32
    std::optional<size_t> find_by_crc32(uint32_t crc) const {
        auto it = m_crc_to_index.find(crc);
        if (it != m_crc_to_index.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    // Записать таблицу в буфер
    size_t write_to_buffer(StaticBuffer* dest, size_t offset);

    size_t size() const { return m_symbols.size(); }
    size_t string_pool_size() const { return m_string_pool.size(); }
};

class StaticBuffer : public HeapObject {

public:
    enum class Endian { Little, Big };

    // Конструктор: привязываем буфер к конкретному типу из TypeSystem
    StaticBuffer(const std::string& type_name, int size, uint32_t origin = 0)
    : m_type_name(type_name), m_origin(origin),
      m_symbol_table(std::make_unique<StaticSymbolTable>()),
      m_labels() 
    {
        m_data.resize(size, 0); // Обнуляем память
    }
    
    void set_endian(Endian e) { m_endian = e; }
    // --- Интерфейс для C++ ---
    uint32_t origin() const { return m_origin; }
    size_t size() const { return m_data.size(); }
    uint8_t* data() { return m_data.data(); }
    const std::string& type_name() const { return m_type_name; }

    Object inspect() const override;
    std::string hex_dump(size_t start_offset = 0, size_t bytes_to_dump = 0,
                            bool show_ascii = true, size_t bytes_per_line = 16) const;

    // Печать для REPL
    std::string print() const override {
        return fmt::format("<static-buffer '{}' :size {} :origin {:#x}>", 
        m_type_name, m_data.size(), m_origin);
    }

    Object make_step_accessor(const Object& key) override;

    // --- Реализация записи различных данных ---

    uint8_t read_u8(size_t offset) {
        return m_data[offset];
    }

    uint16_t read_u16(size_t offset) {
        if (m_endian == Endian::Little) 
            return read_u16_le(offset);
        else 
            return read_u16_be(offset);
    }

    uint32_t read_u32(size_t offset) {
        if (m_endian == Endian::Little) 
            return read_u32_le(offset);
        else 
            return read_u32_be(offset);
    }

    uint64_t read_u64(size_t offset) {
        if (m_endian == Endian::Little) 
            return read_u64_le(offset);
        else 
            return read_u64_be(offset);
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

    void write_u32(size_t offset, uint32_t value) {
        if (m_endian == Endian::Little) 
            write_u32_le(offset, value);
        else 
            write_u32_be(offset, value);
    }

    void write_u64(size_t offset, uint32_t value) {
        if (m_endian == Endian::Little) 
            write_u64_le(offset, value);
        else 
            write_u64_be(offset, value);
    }

    void write_crc32(size_t offset, uint32_t value, int symbol_size) {
        if (symbol_size == 2)
            write_u16(offset, value);
        else
            write_u32(offset, value);
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

    void write_bytes(size_t offset, const uint8_t* data, int len) {
        if (!data || len <= 0) return;
        
        // Проверка, чтобы не выйти за пределы вектора
        if (offset + len > m_data.size()) {
            len = m_data.size() - offset; // Обрезаем, если строка не влезает
        }

        if (len > 0) {
            std::memcpy(m_data.data() + offset, data, len);
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

    public:
    // Добавить символ в таблицу
    uint32_t add_symbol(const std::string& name) {
        if (!m_symbol_table) {
            m_symbol_table = std::make_unique<StaticSymbolTable>();
        }
        size_t index = m_symbol_table->add_symbol(name);
        return m_symbol_table->get_crc32(index);
    }
    
    // Записать таблицу символов в буфер
    size_t write_symbol_table(size_t offset) {
        if (!m_symbol_table || m_symbol_table->size() == 0) {
            return 0;
        }
        
        return m_symbol_table->write_to_buffer(this, offset);
    }
    
    // Получить смещение для таблицы символов
    size_t get_symbol_table_offset() const {
        // Можно положить в конец буфера
        return m_data.size() - calculate_symbol_table_size();
    }

private:

    size_t calculate_symbol_table_size() const {
        if (!m_symbol_table) return 0;
        
        // Заголовок 32 байта + записи + string pool
        return 32 + (m_symbol_table->size() * 8) + m_symbol_table->string_pool_size();
    }

public:

// --- Реализация меток  ---

/**
 * Добавляет метку на определенный офсет.
 * Если метка уже существует, выбрасывает исключение (переопределение метки запрещено).
 */
void add_label(const std::string& name, size_t offset) {
    if (m_labels.find(name) != m_labels.end()) {
        throw std::runtime_error("Label already defined: " + name);
    }
    
    // Проверка границ буфера для безопасности
    if (offset >= m_data.size()) {
        throw std::runtime_error("Label offset out of bounds: " + std::to_string(offset));
    }

    m_labels[name] = offset;
}

/**
 * Возвращает офсет метки по её имени.
 */
size_t get_label_offset(const std::string& name) const {
    auto it = m_labels.find(name);
    if (it == m_labels.end()) {
        throw std::runtime_error("Label not found: " + name);
    }
    return it->second;
}

/**
 * Проверяет наличие метки.
 */
bool has_label(const std::string& name) const {
    return m_labels.find(name) != m_labels.end();
}
    
private:
    // --- 64-битные значения (8 байта) ---

    // --- Little Endian (LE) ---
    uint64_t read_u64_le(size_t offset) const {
        if (offset + 7 >= m_data.size()) return 0; // Или бросить исключение
        return static_cast<uint64_t>(m_data[offset])       |
              (static_cast<uint64_t>(m_data[offset + 1]) << 8)  |
              (static_cast<uint64_t>(m_data[offset + 2]) << 16) |
              (static_cast<uint64_t>(m_data[offset + 3]) << 24) |
              (static_cast<uint64_t>(m_data[offset + 4]) << 32) |
              (static_cast<uint64_t>(m_data[offset + 5]) << 40) |
              (static_cast<uint64_t>(m_data[offset + 6]) << 48) |
              (static_cast<uint64_t>(m_data[offset + 7]) << 56);
    }

    // --- Big Endian (BE) ---
    uint64_t read_u64_be(size_t offset) const {
        if (offset + 7 >= m_data.size()) return 0;
        return (static_cast<uint64_t>(m_data[offset])     << 56) |
               (static_cast<uint64_t>(m_data[offset + 1]) << 48) |
               (static_cast<uint64_t>(m_data[offset + 2]) << 40) |
               (static_cast<uint64_t>(m_data[offset + 3]) << 32) |
               (static_cast<uint64_t>(m_data[offset + 4]) << 24) |
               (static_cast<uint64_t>(m_data[offset + 5]) << 16) |
               (static_cast<uint64_t>(m_data[offset + 6]) << 8)  |
                static_cast<uint64_t>(m_data[offset + 7]);
    }

    // --- 32-битные значения (4 байта) ---

    uint32_t read_u32_le(size_t offset) const {
        if (offset + 3 >= m_data.size()) return 0; // Или бросить исключение
        return static_cast<uint32_t>(m_data[offset]) |
               (static_cast<uint32_t>(m_data[offset + 1]) << 8) |
               (static_cast<uint32_t>(m_data[offset + 2]) << 16) |
               (static_cast<uint32_t>(m_data[offset + 3]) << 24);
    }

    uint32_t read_u32_be(size_t offset) const {
        if (offset + 3 >= m_data.size()) return 0;
        return (static_cast<uint32_t>(m_data[offset]) << 24) |
               (static_cast<uint32_t>(m_data[offset + 1]) << 16) |
               (static_cast<uint32_t>(m_data[offset + 2]) << 8) |
               static_cast<uint32_t>(m_data[offset + 3]);
    }

    // --- 16-битные значения (2 байта) ---

    uint16_t read_u16_le(size_t offset) const {
        if (offset + 1 >= m_data.size()) return 0;
        return static_cast<uint16_t>(m_data[offset]) |
               (static_cast<uint16_t>(m_data[offset + 1]) << 8);
    }

    uint16_t read_u16_be(size_t offset) const {
        if (offset + 1 >= m_data.size()) return 0;
        return (static_cast<uint16_t>(m_data[offset]) << 8) |
               static_cast<uint16_t>(m_data[offset + 1]);
    }

    // --- 64-битные значения (8 байта) ---

    void write_u64_le(size_t offset, uint64_t value) {
        if (offset + 7 >= m_data.size()) return;
        for (int i = 0; i < 8; ++i) {
            m_data[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
        }
    }

    void write_u64_be(size_t offset, uint64_t value) {
        if (offset + 7 >= m_data.size()) return;
        for (int i = 0; i < 8; ++i) {
            // Для BE мы берем байты начиная с самого старшего (сдвиг 56, 48...)
            // и кладем их по порядку в буфер.
            m_data[offset + i] = static_cast<uint8_t>((value >> ((7 - i) * 8)) & 0xFF);
        }
    }

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
    std::string m_type_name;            // Ссылка на тип в TypeSystem
    uint32_t m_origin;                  // Базовый адрес (например, #x2000)
    std::vector<uint8_t> m_data;        // Сырые байты
    Endian m_endian = Endian::Little;   // По умолчанию для Z80
    std::vector<Relocation> m_relocations;
    std::unique_ptr<StaticSymbolTable> m_symbol_table;
    std::unordered_map<std::string, size_t> m_labels;
};


} // namespace script
