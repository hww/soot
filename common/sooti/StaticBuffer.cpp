#include "common/sooti/StaticBuffer.hpp"
#include "common/sooti/Interpreter.hpp" 
#include "common/sooti/Object.hpp"
#include "common/type_system/Type.hpp"


namespace script {

    // Записать таблицу в буфер
    size_t StaticSymbolTable::write_to_buffer(StaticBuffer* dest, size_t offset) {
        const uint32_t MAGIC = 0x53594D54; // "SYMT" в little-endian
        
        // Заголовок
        dest->write_u32(offset, MAGIC);
        dest->write_u32(offset + 4, static_cast<uint32_t>(m_symbols.size()));
        
        // Смещения (вычисляем после записи заголовка)
        uint32_t symbols_offset = 32; // заголовок 32 байта
        uint32_t strings_offset = symbols_offset + 
                                 static_cast<uint32_t>(m_symbols.size() * 8);
        
        dest->write_u32(offset + 8, symbols_offset);
        dest->write_u32(offset + 12, strings_offset);
        
        // Записываем записи символов
        size_t current_offset = offset + symbols_offset;
        for (const auto& symbol : m_symbols) {
            dest->write_u32(current_offset, symbol.crc32);
            dest->write_u32(current_offset + 4, symbol.string_offset);
            current_offset += 8;
        }
        
        // Записываем string pool
        dest->write_bytes(offset + strings_offset, 
                         reinterpret_cast<const uint8_t*>(m_string_pool.data()),
                         m_string_pool.size());
        
        // Общий размер
        size_t total_size = strings_offset + m_string_pool.size();
        return total_size;
    }

// ============================================================================
// StaticBuffer
// ============================================================================

// --- Реализация Accessor для Лиспа ---
void StaticBuffer::define_all_aliases() {
    // Свойства самого буфера
    define_alias("origin", [](Accessor* s) {
        return Object::make_integer(static_cast<StaticBuffer*>(s)->m_origin);
    });
    define_alias("size", [](Accessor* s) {
        return Object::make_integer(static_cast<StaticBuffer*>(s)->m_data.size());
    });
    define_alias("type", [](Accessor* s) {
        return Object::make_symbol(static_cast<StaticBuffer*>(s)->m_type_name);
    });
    // Можно добавить "data", который вернет массив Лиспа, если нужно
}

Object StaticBuffer::inspect() const {
    std::vector<Object> bytes;
    size_t limit = std::min(m_data.size(), (size_t)1024);
    
    for (size_t i = 0; i < limit; ++i) {
        // Форматируем каждый байт как "0xEF" для наглядности
        bytes.push_back(Object::make_integer(m_data[i]));
    }
    
    if (m_data.size() > 16) {
        bytes.push_back(Object::make_symbol("..."));
    }

    return pretty_print::build_list(
        pretty_print::build_list(Object::make_symbol(":type"),   Object::make_string(m_type_name)),
        pretty_print::build_list(Object::make_symbol(":size"),   Object::make_integer(m_data.size())),
        pretty_print::build_list(Object::make_symbol(":origin"), Object::make_integer(m_origin)),
        pretty_print::build_list(Object::make_symbol(":endian"), Object::make_string(m_endian == Endian::Little ? "little" : "big")),
        pretty_print::build_list(Object::make_symbol(":data"),   Object::make_list(bytes))
    );
}

// Новый метод для красивого hex-дампа
std::string StaticBuffer::hex_dump(size_t start_offset, size_t bytes_to_dump,
                        bool show_ascii, size_t bytes_per_line) const {
        if (bytes_to_dump == 0) {
            bytes_to_dump = m_data.size() - start_offset;
        }
        
        if (start_offset >= m_data.size()) {
            return fmt::format("Offset {} exceeds buffer size {}", 
                              start_offset, m_data.size());
        }
        
        size_t end_offset = std::min(start_offset + bytes_to_dump, m_data.size());
        
        std::string result;
        result += fmt::format("Buffer '{}' dump ({} bytes, origin: {:#x}):\n",
                             m_type_name, m_data.size(), m_origin);
        
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


// ============================================================================
// InstanceRef
// ============================================================================

// Магия: пробрасываем доступ к полям структуры через оператор ->
// Мы берем список полей из m_type и для каждого создаем алиас,
// который лезет в m_buffer по (m_offset + field.offset)
void InstanceRef::define_all_aliases() {
    if (!m_type || !m_buffer || !m_ts) return;

    // Захватываем [=, this], чтобы иметь доступ к полям InstanceRef внутри лямбды
    define_alias("_type", [this](Accessor*) {
        return Object::make_symbol(m_type->get_name().c_str());
    });

    for (const auto& field : m_type->fields()) {
        std::string field_name = field.name();
        int field_offset = m_offset + field.offset();
        std::string field_type_name = field.type().base_type();
        bool is_inline = field.is_inline();

        // Захватываем [=, this]:
        // [=] скопирует field_name, field_offset и т.д.
        // [this] даст доступ к m_ts и m_buffer
        define_alias(field_name, [=, this](Accessor*) {
            auto* base_type = m_ts->lookup_type(field_type_name);
            if (!base_type) return Object::make_null();

            if (is_inline) {
                auto* struct_type = dynamic_cast<StructureType*>(base_type);
                if (struct_type) {
                    // ИСПРАВЛЕНИЕ: используем std::make_shared вместо new
                    return Object::make_native_ref(
                        std::make_shared<InstanceRef>(m_ts, m_buffer, field_offset, struct_type)
                    );
                }
            }

            // Логика чтения примитивов
            int load_size = base_type->get_load_size();
            switch (load_size) {
                case 1: return Object::make_integer(m_buffer->read_u8(field_offset));
                case 2: return Object::make_integer(m_buffer->read_u16(field_offset));
                case 4: return Object::make_integer(m_buffer->read_u32(field_offset));
                default: break;
            }

            return Object::make_integer(m_buffer->read_u16(field_offset));
        });
    }
}


std::string InstanceRef::print() const {
    return "<instance " + (m_type ? m_type->get_name() : "unknown") + 
           " @ " + std::to_string(m_offset) + ">";
}
Object InstanceRef::inspect() const {
    // Возвращаем символ или список для дебага в Лиспе
    return Object::make_symbol(m_type->get_name().c_str());
}


} // namespace script
