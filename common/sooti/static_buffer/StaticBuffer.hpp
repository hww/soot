#pragma once

#include <memory>
// Нам нужны полные определения Object и Arguments для сигнатур методов
#include "common/sooti/ListBuilder.hpp"
#include "common/sooti/Object.hpp"
#include "common/type_system/Type.hpp"

#include "fmt/format.h"

class Type;
class TypeSystem;
class StructureType;

namespace script {

class Interpreter;
class EnvironmentObject;

enum class RelocType {
    ABS_ADDR,   // Абсолютный адрес (16/32 бита в зависимости от архитектуры)
    SYMBOL_CRC, // Запись CRC32/16 имени символа
    RELATIVE,   // Относительный адрес (jump/call)
    SYMBOL_TABLE_REF
};

struct Relocation {
    size_t      offset;
    RelocType   type;
    std::string target_name; // Имя типа или символа
};

struct BufferLabel : public HeapObject {
    size_t addr;    // Смещение в буфере
    Object segment; // Имя или объект сегмента (Object для гибкости)
    Object meta;    // Метаданные (asmsym-info из Lisp)

    // Конструктор для удобства
    BufferLabel(size_t a, Object seg, Object m) : addr(a), segment(seg), meta(m) {}

    std::string print() const override {
        return fmt::format("#<buffer-label {:08X} {} {}>", addr, segment.print(), meta.print());
    };

    Object inspect() const override {
        // Создаем Map или список пар для отображения внутреннего состояния
        // Предполагаю, у тебя есть метод создания словаря/карты
        ListBuilder info{};
        info.add_key_value("address", Object::make_integer(addr));
        info.add_key_value("segment", segment);
        info.add_key_value("meta", meta);
        return info.build();
    }

    Object make_step_accessor(const Object &key) override {
        if (key.is_symbol() || key.is_string()) {
            std::string name = key.to_std_string();

            // Позволяем доставать адрес
            if (name == ".address" || name == ".offset") {
                return Object::make_integer(addr);
            }

            // Позволяем доставать сегмент
            if (name == ".segment" || name == ".seg") {
                return segment;
            }

            // Позволяем доставать метаданные
            if (name == ".meta" || name == ".info") {
                return meta;
            }
        }
        throw std::runtime_error("BufferLabel expects .address, .segment or .meta, got " +
                                 key.print());
        // Если ключ не распознан, возвращаем undefined или ошибку
        return Object::make_none();
    }
};

class StaticBuffer;

class StaticSymbolTable {
  private:
    struct SymbolEntry {
        uint32_t    crc32;
        uint32_t    string_offset; // offset в string pool
        std::string name;
    };

    std::vector<SymbolEntry>             m_symbols;
    std::string                          m_string_pool;
    std::unordered_map<uint32_t, size_t> m_crc_to_index; // для быстрого поиска

  public:
    StaticSymbolTable() = default;

    // Добавить символ, возвращает индекс
    size_t add_symbol(const std::string &name) {
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
        size_t      index = m_symbols.size();
        m_symbols.push_back(entry);
        m_crc_to_index[crc] = index;

        return index;
    }

    // Получить CRC32 по индексу
    uint32_t get_crc32(size_t index) const {
        if (index >= m_symbols.size())
            return 0;
        return m_symbols[index].crc32;
    }

    // Получить смещение строки
    uint32_t get_string_offset(size_t index) const {
        if (index >= m_symbols.size())
            return 0;
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
    size_t write_to_buffer(StaticBuffer *dest, size_t offset);

    size_t size() const {
        return m_symbols.size();
    }
    size_t string_pool_size() const {
        return m_string_pool.size();
    }
};

class StaticBuffer : public HeapObject {

  public:
    enum class Endian { Little, Big };

    // Конструктор: привязываем буфер к конкретному типу из TypeSystem
    StaticBuffer(const std::string &type_name, int size, uint32_t origin = 0)
        : m_type_name(type_name), m_origin(origin),
          m_symbol_table(std::make_unique<StaticSymbolTable>()), m_labels() {
        m_data.resize(size, 0); // Обнуляем память
    }

    void set_endian(Endian e) {
        m_endian = e;
    }
    // --- Интерфейс для C++ ---
    uint32_t origin() const {
        return m_origin;
    }
    size_t size() const {
        return m_data.size();
    }
    uint8_t *data() {
        return m_data.data();
    }

    std::string type_name() const override {
        return object_type_to_string(ObjectType::STATIC_BUFFER);
    }

    std::string class_name() const override {
        return "StaticBuffer";
    }
    Object      inspect() const override;
    std::string hex_dump(size_t start_offset = 0, size_t bytes_to_dump = 0, bool show_ascii = true,
                         size_t bytes_per_line = 16) const;

    // Печать для REPL
    std::string print() const override {
        return fmt::format("#<static-buffer '{}' :size {} :origin {:#x}>", m_type_name,
                           m_data.size(), m_origin);
    }

    Object make_step_accessor(const Object &key) override;

    void write_value_at_ptr(void *ptr, Type *type, const Object &val);

    bool is_table() const override {
        return true;
    }

    Object get_at(const Object &key) override;
    void   set_at(const Object &key, const Object &value) override;

    // ============================================================================
    // --- Реализация чтения различных данных ---
    // ============================================================================

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

    // ============================================================================
    // --- Реализация записи различных данных ---
    // ============================================================================

    void write_u8(size_t offset, uint8_t value) {
        m_data[offset] = value;
        update_addr_range(offset, 1);
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
    void write_string(size_t offset, const char *str, int len) {
        if (!str || len <= 0)
            return;

        // Проверка, чтобы не выйти за пределы вектора
        if (offset + len > m_data.size()) {
            len = m_data.size() - offset; // Обрезаем, если строка не влезает
        }

        if (len > 0) {
            std::memcpy(m_data.data() + offset, str, len);
        }
        update_addr_range(offset, len);
    }

    /**
     * Записывает std::string целиком.
     * По умолчанию добавляет нулевой терминатор (null-terminated string).
     */
    void write_string(size_t offset, const std::string &str, bool null_terminated = true) {
        int len = static_cast<int>(str.length());
        write_string(offset, str.c_str(), len);

        if (null_terminated) {
            size_t term_offset = offset + len;
            if (term_offset < m_data.size()) {
                m_data[term_offset] = 0;
            }
        }
    }

    void write_bytes(size_t offset, const uint8_t *data, int len) {
        if (!data || len <= 0)
            return;

        // Проверка, чтобы не выйти за пределы вектора
        if (offset + len > m_data.size()) {
            len = m_data.size() - offset; // Обрезаем, если строка не влезает
        }

        if (len > 0) {
            std::memcpy(m_data.data() + offset, data, len);
        }
        update_addr_range(offset, len);
    }

    // ============================================================================
    // --- Реализация реалокиции указателей ---
    // ============================================================================

  public:
    // API для добавления релокации
    void add_reloc(size_t offset, RelocType type, const std::string &target) {
        m_relocations.push_back({offset, type, target});
    }

    // Получить все релокации для линковщика
    const std::vector<Relocation> &get_relocations() const {
        return m_relocations;
    }

    // Очистить (если нужно пересобрать буфер)
    void clear_relocs() {
        m_relocations.clear();
    }

  public:
    // Добавить символ в таблицу
    uint32_t add_symbol(const std::string &name) {
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
        if (!m_symbol_table)
            return 0;

        // Заголовок 32 байта + записи + string pool
        return 32 + (m_symbol_table->size() * 8) + m_symbol_table->string_pool_size();
    }

    // ============================================================================
    // --- Реализация меток  ---
    // ============================================================================

  public:
    /**
     * Добавляет новую метку. Вызывается только если метки нет.
     */
    void add_label(const std::string &name, size_t offset, Object segment, Object meta) {
        // 1. Создаем объект Label в куче и оборачиваем в shared_ptr
        auto label_ptr = std::make_shared<BufferLabel>(offset, segment, meta);

        // 2. Создаем Object типа NATIVE_REF, который владеет этим shared_ptr
        Object new_label = Object::make_heap_obj(std::move(label_ptr));

        // 3. Сохраняем Object в мапу (мапа теперь держит сильную ссылку на объект)
        m_labels[name] = new_label;
    }

    /**
     * Возвращает Object, который уже содержит shared_ptr<Label>
     */
    Object get_label_obj(const std::string &name) const {
        auto it = m_labels.find(name);
        if (it != m_labels.end()) {
            return it->second; // Просто копируем Object (копируется shared_ptr внутри)
        }
        return Object::make_null(); // Возвращаем пустой объект
    }

    /**
     * Вспомогательный метод для проверки существования
     */
    bool has_label(const std::string &name) const {
        return m_labels.find(name) != m_labels.end();
    }

    /**
     * Позволяет получить все метаданные для экспорта в символы (.sym.sot)
     */
    const std::unordered_map<std::string, Object> &get_all_labels() const {
        return m_labels;
    }

    // ============================================================================
    // --- Реализация указателей  ---
    // ============================================================================

  public:
    /**
     * Записывает релоцируемое значение.
     * @param offset Куда пишем в буфере
     * @param target Имя метки или символа, на который ссылаемся
     * @param type Тип релокации (абсолютный адрес, относительный и т.д.)
     * @param size Размер места под адрес (обычно 2 для 16-бит или 4 для 32-бит)
     */
    void write_reloc(size_t offset, const std::string &target, RelocType type) {
        // 1. Создаем запись о релокации
        add_reloc(offset, type, target);

        // 2. Записываем временную заглушку (0), чтобы место было зарезервировано
        write_pointer(offset, 0);
    }

    void write_pointer(size_t offset, uint64_t value) {
        switch (TypeConfig::pointer_size) {
        case 1:
            write_u8(offset, value);
            break;
        case 2:
            write_u16(offset, value);
            break;
        case 4:
            write_u32(offset, value);
            break;
        default:
            write_u64(offset, value);
            break;
        }
    }

    // ============================================================================
    // Линковка
    // ============================================================================

    void link_internal() {
        for (auto &reloc : m_relocations) {
            // 1. Ищем объект метки в мапе
            Object label_obj = get_label_obj(reloc.target_name);
            if (label_obj.is_null())
                continue;

            // 2. Достаем указатель на HeapObject Label
            // Используем as_heap<Label>(), так как это NATIVE_REF, указывающий на Label
            auto label = label_obj.as_heap_obj<BufferLabel>();

            // 3. Вычисляем финальный адрес с учетом базы (origin)
            size_t target_addr = label->addr + m_origin;

            if (reloc.type == RelocType::ABS_ADDR) {
                write_pointer(reloc.offset, target_addr);
            } else if (reloc.type == RelocType::RELATIVE) {
                // Расчет относительного прыжка (например, для JR или CALL)
                // Учитываем размер самого указателя, так как PC обычно указывает на следующий байт
                int64_t diff = static_cast<int64_t>(target_addr) -
                               static_cast<int64_t>(reloc.offset + TypeConfig::pointer_size);

                write_pointer(reloc.offset, static_cast<uint64_t>(diff));
            }
        }
    }

    // ============================================================================
    // --- Размер занятого пространства ---
    // ============================================================================

  public:
    uint32_t get_start_addr() const {
        return (m_addr_min == 0xFFFFFFFF) ? 0 : m_addr_min;
    }
    uint32_t get_end_addr() const {
        return (m_addr_min == 0xFFFFFFFF) ? 0 : m_addr_max;
    }
    size_t get_filled_size() const {
        return (m_addr_min == 0xFFFFFFFF) ? 0 : (m_addr_max - m_addr_min + 1);
    }

    void update_addr_range(uint32_t addr, uint32_t size) {
        if (m_addr_min == 0xFFFFFFFF) {
            m_addr_min = addr;
            m_addr_max = addr;
        } else {
            m_addr_min = std::min(m_addr_min, addr);
            m_addr_max = std::max(m_addr_max, addr + size - 1);
        }
    }

    // ============================================================================
    // --- Запись другого буфера ---
    // ============================================================================

    void write_buffer(size_t offset, StaticBuffer *other) {
        // 1. Копируем сырые байты
        if (offset + other->size() > m_data.size()) {
            m_data.resize(offset + other->size()); // Расширяем, если нужно
        }
        std::memcpy(m_data.data() + offset, other->data(), other->size());
        update_addr_range(offset, other->size());
        // 2. Переносим релокации с коррекцией смещения
        for (const auto &reloc : other->get_relocations()) {
            this->add_reloc(offset + reloc.offset, reloc.type, reloc.target_name);
        }

        // 3. Переносим метки
        for (const auto &[name, label_obj] : other->get_all_labels()) {
            auto label = label_obj.as_heap_obj<BufferLabel>();
            this->add_label(name, offset + label->addr, label->segment, label->meta);
        }
    }

    // ============================================================================
    // --- 64-битные значения (8 байта) ---
    // ============================================================================

  private:
    // --- Little Endian (LE) ---
    uint64_t read_u64_le(size_t offset) const {
        if (offset + 7 >= m_data.size())
            return 0; // Или бросить исключение
        return static_cast<uint64_t>(m_data[offset]) |
               (static_cast<uint64_t>(m_data[offset + 1]) << 8) |
               (static_cast<uint64_t>(m_data[offset + 2]) << 16) |
               (static_cast<uint64_t>(m_data[offset + 3]) << 24) |
               (static_cast<uint64_t>(m_data[offset + 4]) << 32) |
               (static_cast<uint64_t>(m_data[offset + 5]) << 40) |
               (static_cast<uint64_t>(m_data[offset + 6]) << 48) |
               (static_cast<uint64_t>(m_data[offset + 7]) << 56);
    }

    // --- Big Endian (BE) ---
    uint64_t read_u64_be(size_t offset) const {
        if (offset + 7 >= m_data.size())
            return 0;
        return (static_cast<uint64_t>(m_data[offset]) << 56) |
               (static_cast<uint64_t>(m_data[offset + 1]) << 48) |
               (static_cast<uint64_t>(m_data[offset + 2]) << 40) |
               (static_cast<uint64_t>(m_data[offset + 3]) << 32) |
               (static_cast<uint64_t>(m_data[offset + 4]) << 24) |
               (static_cast<uint64_t>(m_data[offset + 5]) << 16) |
               (static_cast<uint64_t>(m_data[offset + 6]) << 8) |
               static_cast<uint64_t>(m_data[offset + 7]);
    }

    // ============================================================================
    // --- 32-битные значения (4 байта) ---
    // ============================================================================

    uint32_t read_u32_le(size_t offset) const {
        if (offset + 3 >= m_data.size())
            return 0; // Или бросить исключение
        return static_cast<uint32_t>(m_data[offset]) |
               (static_cast<uint32_t>(m_data[offset + 1]) << 8) |
               (static_cast<uint32_t>(m_data[offset + 2]) << 16) |
               (static_cast<uint32_t>(m_data[offset + 3]) << 24);
    }

    uint32_t read_u32_be(size_t offset) const {
        if (offset + 3 >= m_data.size())
            return 0;
        return (static_cast<uint32_t>(m_data[offset]) << 24) |
               (static_cast<uint32_t>(m_data[offset + 1]) << 16) |
               (static_cast<uint32_t>(m_data[offset + 2]) << 8) |
               static_cast<uint32_t>(m_data[offset + 3]);
    }

    // ============================================================================
    // --- 16-битные значения (2 байта) ---
    // ============================================================================

    uint16_t read_u16_le(size_t offset) const {
        if (offset + 1 >= m_data.size())
            return 0;
        return static_cast<uint16_t>(m_data[offset]) |
               (static_cast<uint16_t>(m_data[offset + 1]) << 8);
    }

    uint16_t read_u16_be(size_t offset) const {
        if (offset + 1 >= m_data.size())
            return 0;
        return (static_cast<uint16_t>(m_data[offset]) << 8) |
               static_cast<uint16_t>(m_data[offset + 1]);
    }

    // ============================================================================
    // --- 64-битные значения (8 байта) ---
    // ============================================================================

    void write_u64_le(size_t offset, uint64_t value) {
        if (offset + 7 >= m_data.size())
            return;
        for (int i = 0; i < 8; ++i) {
            m_data[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
        }
        update_addr_range(offset, 8);
    }

    void write_u64_be(size_t offset, uint64_t value) {
        if (offset + 7 >= m_data.size())
            return;
        for (int i = 0; i < 8; ++i) {
            // Для BE мы берем байты начиная с самого старшего (сдвиг 56, 48...)
            // и кладем их по порядку в буфер.
            m_data[offset + i] = static_cast<uint8_t>((value >> ((7 - i) * 8)) & 0xFF);
        }
        update_addr_range(offset, 8);
    }

    // ============================================================================
    // --- 32-битные значения (4 байта) ---
    // ============================================================================

    void write_u32_le(size_t offset, uint32_t value) {
        if (offset + 3 >= m_data.size())
            return; // Защита от выхода за границы
        m_data[offset] = (value & 0x000000FF);
        m_data[offset + 1] = (value & 0x0000FF00) >> 8;
        m_data[offset + 2] = (value & 0x00FF0000) >> 16;
        m_data[offset + 3] = (value & 0xFF000000) >> 24;
        update_addr_range(offset, 4);
    }

    void write_u32_be(size_t offset, uint32_t value) {
        if (offset + 3 >= m_data.size())
            return;
        m_data[offset] = (value & 0xFF000000) >> 24;
        m_data[offset + 1] = (value & 0x00FF0000) >> 16;
        m_data[offset + 2] = (value & 0x0000FF00) >> 8;
        m_data[offset + 3] = (value & 0x000000FF);
        update_addr_range(offset, 4);
    }

    // ============================================================================
    // --- 16-битные значения (2 байта) ---
    // ============================================================================

    void write_u16_le(size_t offset, uint16_t value) {
        if (offset + 1 >= m_data.size())
            return;
        m_data[offset] = (value & 0xFF);
        m_data[offset + 1] = (value >> 8) & 0xFF;
        update_addr_range(offset, 2);
    }

    void write_u16_be(size_t offset, uint16_t value) {
        if (offset + 1 >= m_data.size())
            return;
        m_data[offset] = (value >> 8) & 0xFF;
        m_data[offset + 1] = (value & 0xFF);
        update_addr_range(offset, 2);
    }

  private:
    uint32_t                                m_addr_min = 0xFFFFFFFF; // Максимально возможное
    uint32_t                                m_addr_max = 0;
    std::string                             m_type_name; // Ссылка на тип в TypeSystem
    uint32_t                                m_origin;    // Базовый адрес (например, #x2000)
    std::vector<uint8_t>                    m_data;      // Сырые байты
    Endian                                  m_endian = Endian::Little; // По умолчанию для Z80
    std::vector<Relocation>                 m_relocations;
    std::unique_ptr<StaticSymbolTable>      m_symbol_table;
    std::unordered_map<std::string, Object> m_labels;
};

} // namespace script
