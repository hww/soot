#include "StaticBuffer.hpp"
#include "common/sooti/Interpreter.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/static_buffer/TypePointer.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "sooti/static_buffer/StaticBuffer.hpp"
#include "sooti/static_buffer/StaticBufferWriter.hpp"
namespace script {

// Записать таблицу в буфер
size_t StaticSymbolTable::write_to_buffer(StaticBuffer *dest, size_t offset) {
    const uint32_t MAGIC = 0x53594D54; // "SYMT" в little-endian

    // Заголовок
    dest->write_u32(offset, MAGIC);
    dest->write_u32(offset + 4, static_cast<uint32_t>(m_symbols.size()));

    // Смещения (вычисляем после записи заголовка)
    uint32_t symbols_offset = 32; // заголовок 32 байта
    uint32_t strings_offset = symbols_offset + static_cast<uint32_t>(m_symbols.size() * 8);

    dest->write_u32(offset + 8, symbols_offset);
    dest->write_u32(offset + 12, strings_offset);

    // Записываем записи символов
    size_t current_offset = offset + symbols_offset;
    for (const auto &symbol : m_symbols) {
        dest->write_u32(current_offset, symbol.crc32);
        dest->write_u32(current_offset + 4, symbol.string_offset);
        current_offset += 8;
    }

    // Записываем string pool
    dest->write_bytes(offset + strings_offset,
                      reinterpret_cast<const uint8_t *>(m_string_pool.data()),
                      m_string_pool.size());

    // Общий размер
    size_t total_size = strings_offset + m_string_pool.size();
    return total_size;
}

// ============================================================================
// StaticBuffer
// ============================================================================

Object StaticBuffer::make_step_accessor(const Object &key) {
    // 1. Системные свойства
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        // 1. Системные свойства
        if (name == ".size")
            return Object::make_integer(size());
        if (name == ".origin")
            return Object::make_integer(origin());
        if (name == ".type")
            return Object::make_string(type_name());
        if (name == ".start-addr")
            return Object::make_integer(get_start_addr());
        if (name == ".end-addr")
            return Object::make_integer(get_end_addr());
        if (name == ".filled-size")
            return Object::make_integer(get_end_addr() - get_start_addr());
        // Внутри StaticBuffer::make_step_accessor
        if (name == ".labels") {
            std::vector<Object> names;
            // Предполагаем, что у тебя есть m_labels или аналогичная структура
            // (std::map/unordered_map)
            for (auto const &[label_name, label_obj] : m_labels) {
                names.push_back(Object::make_string(label_name));
            }
            return Object::make_list(names); // Возвращаем как обычный Lisp-список
        }

        // 2. Умный доступ по метке
        Object label_obj = get_label_obj(name);
        if (label_obj.is_not_null())
            return label_obj;

        return Object::make_null();
    }

    // 3. Низкоуровневый доступ по оффсету (например: (buffer 10))
    if (key.is_integer()) {
        Type *byte_t = TypeSystem::instance().lookup_type("uint8");
        if (!byte_t)
            throw std::runtime_error("TypeSystem: uint8 not found");

        size_t offset = static_cast<size_t>(key.as_integer());

        if (offset >= size())
            return Object::make_none();

        uint8_t *next_ptr = this->data() + offset;

        // ИСПРАВЛЕНИЕ: Только 3 аргумента (ptr, type, owner)
        auto b_cell = std::make_shared<TypePointer>(next_ptr,          // Физический адрес
                                                    byte_t,            // Тип (uint8)
                                                    shared_from_this() // Владелец
        );

        return Object::make_heap_obj(b_cell, ObjectType::POINTER);
    }

    return Object::make_none();
}

Object StaticBuffer::get_at(const Object &key) {
    // 3. Низкоуровневый доступ по оффсету (например: (buffer 10))
    if (key.is_integer()) {
        Type *byte_t = TypeSystem::instance().lookup_type("uint8");
        if (!byte_t)
            throw std::runtime_error("TypeSystem: uint8 not found");

        size_t offset = static_cast<size_t>(key.as_integer());

        if (offset >= size())
            return Object::make_none();

        uint8_t *next_ptr = this->data() + offset;

        // ИСПРАВЛЕНИЕ: Только 3 аргумента (ptr, type, owner)
        auto b_cell = std::make_shared<TypePointer>(next_ptr,          // Физический адрес
                                                    byte_t,            // Тип (uint8)
                                                    shared_from_this() // Владелец
        );

        return Object::make_heap_obj(b_cell, ObjectType::POINTER);
    }
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();
        Object      label_obj = get_label_obj(name);
        if (label_obj.is_not_null())
            return label_obj;
    }
    return Object::make_none();
}

void StaticBuffer::set_at(const Object &key, const Object &value) {
    // 1. Если ключ — число (оффсет), пишем как uint8 (по аналогии с get_at)
    if (key.is_integer()) {
        size_t offset = static_cast<size_t>(key.as_integer());
        if (offset >= size()) {
            throw std::runtime_error("StaticBuffer::set_at: offset out of bounds");
        }

        Type *byte_t = TypeSystem::instance().lookup_type("uint8");
        // Используем уже готовую физику буфера
        this->write_value_at_ptr(this->data() + offset, byte_t, value);
        return;
    }

    // 2. Если ключ — символ (метка)
    if (key.is_symbol() || key.is_string()) {
        Object target = this->get_at(key); // Пытаемся найти указатель по метке
        if (target.is_pointer()) {
            target.as_pointer()->set(value);
            return;
        }
    }

    throw std::runtime_error("StaticBuffer::set_at: invalid key or target not writable");
}
void StaticBuffer::write_value_at_ptr(void *ptr, Type *type, const Object &val) {
    /*
    printf("PHYSICAL WRITE: data=%p ptr=%p, type=%s, val=%s\n", m_data.data(), ptr,
           type->get_name().c_str(), val.print().c_str());
    */
    if (!type || !ptr) {
        throw std::runtime_error(fmt::format("CRITICAL: type is null at ptr {:p}\n", ptr));
        return;
    }

    // 1. Вычисляем смещение относительно начала данных буфера
    uint8_t *byte_ptr = static_cast<uint8_t *>(ptr);
    uint8_t *base_ptr = static_cast<uint8_t *>(this->data());

    // Безопасная проверка: находится ли указатель внутри нашего буфера?
    if (byte_ptr < base_ptr || byte_ptr >= base_ptr + this->size()) {
        throw std::runtime_error("StaticBuffer: attempt to write outside of buffer boundaries");
    }

    size_t offset = byte_ptr - base_ptr;
    size_t size = type->get_size_in_memory();

    // 2. Выполняем физическую запись через статический хелпер
    StaticBufferWriter::write_value_at_ptr(ptr, type, val);

    // 3. Обновляем "грязный" диапазон (для синхронизации с GPU или сохранения)
    this->update_addr_range(offset, size);
}

Object StaticBuffer::inspect() const {
    std::vector<Object> bytes;
    size_t              limit = std::min(m_data.size(), (size_t)1024);

    for (size_t i = 0; i < limit; ++i) {
        // Форматируем каждый байт как "0xEF" для наглядности
        bytes.push_back(Object::make_integer(m_data[i]));
    }

    if (m_data.size() > 16) {
        bytes.push_back(Object::make_symbol("..."));
    }

    return pretty_print::build_list(
        pretty_print::build_list(Object::make_symbol(":type"), Object::make_string(m_type_name)),
        pretty_print::build_list(Object::make_symbol(":size"), Object::make_integer(m_data.size())),
        pretty_print::build_list(Object::make_symbol(":origin"), Object::make_integer(m_origin)),
        pretty_print::build_list(
            Object::make_symbol(":endian"),
            Object::make_string(m_endian == Endian::Little ? "little" : "big")),
        pretty_print::build_list(Object::make_symbol(":labels"),
                                 Object::make_integer(m_labels.size())),
        pretty_print::build_list(Object::make_symbol(":data"), Object::make_list(bytes)));
}

// Новый метод для красивого hex-дампа
std::string StaticBuffer::hex_dump(size_t start_offset, size_t bytes_to_dump, bool show_ascii,
                                   size_t bytes_per_line) const {
    if (bytes_to_dump == 0) {
        bytes_to_dump = m_data.size() - start_offset;
    }

    if (start_offset >= m_data.size()) {
        return fmt::format("Offset {} exceeds buffer size {}", start_offset, m_data.size());
    }

    size_t end_offset = std::min(start_offset + bytes_to_dump, m_data.size());

    std::string result = "";
    result += fmt::format("Buffer: '{}' | dump ({} bytes, origin: {:#x}):\n", m_type_name,
                          m_data.size(), m_origin);

    for (size_t offset = start_offset; offset < end_offset; offset += bytes_per_line) {
        size_t line_end = std::min(offset + bytes_per_line, end_offset);

        // Адрес
        result += fmt::format("{:08x}: ", offset + m_origin);

        // Hex байты
        for (size_t i = offset; i < line_end; i++) {
            result += fmt::format("{:02x} ", m_data[i]);
        }

        // Заполнение для выравнивания
        for (size_t i = line_end; i < offset + bytes_per_line; i++) {
            result += "   ";
        }

        // ASCII представление (опционально)
        if (show_ascii) {
            result += " |";
            for (size_t i = offset; i < line_end; i++) {
                uint8_t byte = m_data[i];
                if (byte >= 32 && byte < 127) {
                    result += static_cast<char>(byte);
                } else {
                    result += '.';
                }
            }
            result += '|';
        }
        result += '\n';
    }

    return result;
}

} // namespace script
