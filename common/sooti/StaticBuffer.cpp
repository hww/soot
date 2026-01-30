#include "common/sooti/StaticBuffer.hpp"
#include "common/sooti/Interpreter.hpp" 
#include "common/sooti/Object.hpp"
#include "common/type_system/Type.hpp"


namespace script {

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
    size_t limit = std::min(m_data.size(), (size_t)16);
    
    for (size_t i = 0; i < limit; ++i) {
        // Форматируем каждый байт как "0xEF" для наглядности
        bytes.push_back(Object::make_string(fmt::format("{:#04x}", m_data[i])));
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
