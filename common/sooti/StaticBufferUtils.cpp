#include "StaticBufferUtils.hpp"
#include "Printer.hpp"
#include <cstring>
#include <stdexcept>
#include <unordered_map>

namespace script {

// ========================================================================
// Вспомогательные методы
// ========================================================================

void StaticBufferUtils::check_bounds(StaticBuffer* buf, size_t offset, size_t size,
                                    const std::string& operation, const std::string& type_name) {
    if (!buf) {
        throw std::runtime_error("Buffer is null");
    }
    
    if (offset + size > buf->size()) {
        throw std::runtime_error(fmt::format(
            "{} overflow for type '{}': offset {} + size {} > buffer size {}",
            operation, type_name, offset, size, buf->size()
        ));
    }
}

Type* StaticBufferUtils::get_field_type(TypeSystem* ts, const Field& field) {
    try {
        return ts->lookup_type(field.type());
    }
    catch (const std::exception&) {
        return nullptr;
    }
}

bool StaticBufferUtils::is_compatible_types(TypeSystem* ts, Type* type1, Type* type2) {
    // Упрощенная проверка - можно улучшить
    return type1->get_name() == type2->get_name();
}

// ========================================================================
// Основные методы записи (возвращают размер)
// ========================================================================
size_t StaticBufferUtils::write_recursive(TypeSystem* ts, StaticBuffer* dest,
                                         size_t offset, Type* type, const Object& source) {
    if (!ts || !dest || !type) {
        throw std::runtime_error("StaticBufferUtils: null arguments");
    }
    
    size_t total_written_by_type = 0;
    size_t total_written_by_data = 0;
    
    // 1. Сначала подготавливаем память под этот тип (зануляем, пишем теги)
    total_written_by_type += write_from_type(ts, dest, offset, type);
    
    if (source.is_null()) {
        return total_written_by_type; // Оставляем значения по умолчанию
    }
    
    // 2. Определяем источник и наполняем
    if (source.is_native_ref()) {
        auto src_buf = source.as_native_ref<StaticBuffer>();
        if (src_buf) {
            total_written_by_data += write_from_buffer(ts, dest, offset, type, src_buf.get());
        }
    } else {
        total_written_by_data += write_from_data(ts, dest, offset, type, source);
    }
    
    return total_written_by_type;
}

size_t StaticBufferUtils::write_from_type(TypeSystem* ts, StaticBuffer* dest,
                                         size_t offset, Type* type) {
    if (!type || !dest) return 0;
    
    size_t size = type->get_size_in_memory();
    check_bounds(dest, offset, size, "write_from_type", type->get_name());
    
    // Базовое зануление (важно для padding!)
    std::memset(dest->data() + offset, 0, size);
    
    // Для basic типов пишем тип-тег
    if (type->get_class_name() == "basic") {
        auto& config = ts->get_config();
        uint32_t type_tag = type->get_type_tag();
        
        if (config.crc_value_size == 4) {
            dest->write_u32(offset, type_tag);
        } else if (config.crc_value_size == 2) {
            dest->write_u16(offset, static_cast<uint16_t>(type_tag));
        }
        
        // Для некоторых basic типов есть heap_base
        if (type->heap_base() != 0 && offset + 8 <= dest->size()) {
            dest->write_u32(offset + 4, type->heap_base());
        }
    }
    
    return size; // Возвращаем размер типа
}

size_t StaticBufferUtils::write_from_buffer(TypeSystem* ts, StaticBuffer* dest,
                                           size_t offset, Type* type, StaticBuffer* src) {
    if (!src) {
        throw std::runtime_error("Source buffer is null");
    }
    
    size_t size = type->get_size_in_memory();
    check_bounds(dest, offset, size, "write_from_buffer", type->get_name());
    
    if (src->size() < size) {
        throw std::runtime_error(fmt::format(
            "Source buffer too small: {} < {}", src->size(), size
        ));
    }
    
    // Простое копирование байтов
    std::memcpy(dest->data() + offset, src->data(), size);
    
    // Копируем релокации с поправкой на смещение
    for (const auto& reloc : src->get_relocations()) {
        Relocation new_reloc = reloc;
        new_reloc.offset += offset;
        
        // Проверяем, что релокация в пределах буфера
        if (new_reloc.offset + 4 <= dest->size()) {
            dest->add_reloc(new_reloc.offset, new_reloc.type, new_reloc.target_name);
        }
    }
    
    return size; // Возвращаем скопированный размер
}

size_t StaticBufferUtils::write_from_data(TypeSystem* ts, StaticBuffer* dest,
                                         size_t offset, Type* type, const Object& source) {
    if (!type) return 0;
    
    std::string type_name = type->get_name();
    
    // 1. Сначала специальные типы с особой семантикой
    if (type_name == "string" || type_name == "symbol") {
        return write_string_data(ts, dest, offset, type, source);
    }
    
    std::string class_name = type->get_class_name();
    
    // 2. Затем общие категории типов
    if (class_name == "value") {
        auto* value_type = dynamic_cast<ValueType*>(type);
        if (value_type) {
            return write_value_data(ts, dest, offset, value_type, source);
        }
    }
    else if (class_name == "structure" || class_name == "basic") {
        auto* struct_type = dynamic_cast<StructureType*>(type);
        if (struct_type) {
            return write_structure_data(ts, dest, offset, struct_type, source);
        }
    }
    else if (class_name == "enum") {
        auto* enum_type = dynamic_cast<EnumType*>(type);
        if (enum_type) {
            return write_enum_data(ts, dest, offset, enum_type, source);
        }
    }
    else if (class_name == "bitfield") {
        auto* bitfield_type = dynamic_cast<BitFieldType*>(type);
        if (bitfield_type) {
            return write_bitfield_data(ts, dest, offset, bitfield_type, source);
        }
    }
    else {
        throw std::runtime_error("Unsupported type: " + type_name);
    }
    
    return 0;
}

// ========================================================================
// Запись строк и символов (возвращает ФАКТИЧЕСКИЙ размер)
// ========================================================================

size_t StaticBufferUtils::write_string_data(TypeSystem* ts, StaticBuffer* dest,
                                           size_t offset, Type* type, const Object& source) {
    if (!source.is_string() && !source.is_symbol()) {
        throw std::runtime_error("Expected string or symbol");
    }

    std::string str = source.to_std_string();
    size_t total_written = 0;
    
    if (type->get_name() == "string") {
        // Для Z80 string - это структура! Используем write_structure_data
        
        // 1. Получаем структуру string
        auto* string_type = dynamic_cast<StructureType*>(type);
        if (!string_type) {
            throw std::runtime_error("string type is not a StructureType");
        }
        
        // 2. Проверяем поля string
        Field length_field;
        Field data_field;
        
        bool has_length = string_type->lookup_field("length", &length_field);
        bool has_data = string_type->lookup_field("data", &data_field);
        
        fmt::print("[DEBUG] String fields: length={}, data={}\n",
                  has_length, has_data);
        
        // 3. Создаем alist для записи
        Object alist = Object::make_null();
        
        if (has_length) {
            // Добавляем length поле
            Object length_pair = Object::make_pair(
                Object::make_symbol("length"),
                Object::make_integer(str.length())
            );
            alist = Object::make_pair(length_pair, alist);
        }
        
        // 4. Записываем структуру
        size_t struct_written = write_structure_data(ts, dest, offset, string_type, alist);
        
        // 5. Записываем данные строки
        if (has_data) {
            if (data_field.is_inline()) {
                // Данные inline в структуре
                Type* data_type = ts->lookup_type(data_field.type());
                if (data_type && data_type->get_name() == "uint8") {
                    // Записываем в поле data структуры
                    size_t data_offset = offset + data_field.offset();
                    dest->write_string(data_offset, str, true);
                    fmt::print("[DEBUG] Inline string data at offset {}\n", data_offset);
                }
            } else {
                // Данные dynamic - после структуры
                size_t data_offset = offset + string_type->get_size_in_memory();
                dest->write_string(data_offset, str, true);
                fmt::print("[DEBUG] Dynamic string data at offset {}\n", data_offset);
                
                // Для dynamic данных возвращаем полный размер
                return struct_written + str.length() + 1;
            }
        } else {
            // Нет поля data - записываем просто после структуры
            size_t data_offset = offset + string_type->get_size_in_memory();
            dest->write_string(data_offset, str, true);
            fmt::print("[DEBUG] String data after struct at offset {}\n", data_offset);
            
            return struct_written + str.length() + 1;
        }
        
        return struct_written;
    }
    else if (type->get_name() == "symbol") {
        // Для символа: структура символа + имя
        size_t struct_size = 24; // symbol struct size
        size_t name_size = str.length() + 1;
        
        check_bounds(dest, offset, struct_size + name_size,
                    "write_symbol", type->get_name());
        
        uint32_t crc32 = dest->add_symbol(str);
        auto sym_size = ts->get_config().crc_value_size;
        
        if (type->get_class_name() == "basic") {
            // Basic header
            dest->write_crc32(offset, type->get_type_tag(), sym_size);
            dest->write_u32(offset + 4, 0);
            
            // Symbol fields
            dest->write_crc32(offset + 8, crc32, sym_size); // name pointer (CRC)
            dest->write_crc32(offset + 12, 0, sym_size);    // value
            dest->write_crc32(offset + 16, 0, sym_size);    // package
            dest->write_crc32(offset + 20, 0, sym_size);    // plist
        } else {
            // Просто CRC32
            dest->write_crc32(offset, crc32, sym_size);
        }
        
        // Имя символа (после структуры)
        size_t name_offset = offset + struct_size;
        dest->write_string(name_offset, str, true);
        
        // Релокация
        dest->add_reloc(offset, RelocType::SYMBOL_TABLE_REF, str);
        
        total_written = struct_size + name_size;
    }
    
    return total_written;
}

// ========================================================================
// Запись примитивных типов (возвращает размер типа)
// ========================================================================

size_t StaticBufferUtils::write_value_data(TypeSystem* ts, StaticBuffer* dest,
                                          size_t offset, ValueType* type, const Object& source) {
    if (!type) return 0;
    
    int size = type->get_load_size();
    bool is_signed = type->get_load_signed();
    
    // ВАЖНО: проверяем границы по размеру загрузки, а не по размеру в памяти
    check_bounds(dest, offset, type->get_size_in_memory(), 
                "write_value", type->get_name());
    
    if (source.is_integer()) {
        int64_t value = source.as_integer();
        
        if (size == 1) {
            if (is_signed) {
                dest->write_u8(offset, static_cast<uint8_t>(static_cast<int8_t>(value)));
            } else {
                dest->write_u8(offset, static_cast<uint8_t>(value));
            }
        }
        else if (size == 2) {
            if (is_signed) {
                dest->write_u16(offset, static_cast<uint16_t>(static_cast<int16_t>(value)));
            } else {
                dest->write_u16(offset, static_cast<uint16_t>(value));
            }
        }
        else if (size == 4) {
            if (is_signed) {
                dest->write_u32(offset, static_cast<uint32_t>(static_cast<int32_t>(value)));
            } else {
                dest->write_u32(offset, static_cast<uint32_t>(value));
            }
        }
        else if (size == 8) {
            dest->write_u64(offset, static_cast<uint64_t>(value));
        }
    }
    else if (source.is_float()) {
        double value = source.as_float();
        
        if (size == 4) {
            float float_value = static_cast<float>(value);
            uint32_t bits;
            std::memcpy(&bits, &float_value, 4);
            dest->write_u32(offset, bits);
        }
        else if (size == 8) {
            uint64_t bits;
            std::memcpy(&bits, &value, 8);
            dest->write_u64(offset, bits);
        }
    }
    else {
        throw std::runtime_error("Expected number for value type");
    }
    
    // ВОЗВРАЩАЕМ РАЗМЕР ТИПА В ПАМЯТИ
    return type->get_size_in_memory();
}

// ========================================================================
// Запись структур (возвращает размер структуры)
// ========================================================================

size_t StaticBufferUtils::write_structure_data(TypeSystem* ts, StaticBuffer* dest,
                                              size_t offset, StructureType* type, const Object& source) {
    if (!type) return 0;
    
    size_t struct_size = type->get_size_in_memory();
    check_bounds(dest, offset, struct_size, "write_structure", type->get_name());
    
    // 1. Сначала базовая инициализация
    write_from_type(ts, dest, offset, type);
    
    if (source.is_null()) {
        return struct_size; // Записали только базовую инициализацию
    }
    
    // 2. Записываем поля из source
    size_t fields_written = 0;
    
    if (source.is_list()) {
        // Если источник - alist
        fields_written = write_from_alist(ts, dest, offset, type, source);
    } 
    else if (source.is_native_ref()) {
        // Если источник - другой буфер
        auto src_buf = source.as_native_ref<StaticBuffer>();
        if (src_buf) {
            fields_written = write_from_buffer(ts, dest, offset, type, src_buf.get());
        }
    }
    else if (source.is_static_buffer()) {
        // Если источник - объект структуры (нужно добавить этот тип в Object!)
        // fields_written = write_from_structure_object(...);
    }
    
    // Возвращаем размер структуры (не суммарный размер полей)
    return struct_size;
}

size_t StaticBufferUtils::write_from_alist(TypeSystem* ts, StaticBuffer* dest,
                                         size_t offset, StructureType* type, const Object& alist) {
    size_t total_written = 0;
    
    // Строим map полей из alist
    std::unordered_map<std::string, Object> field_values;
    Object current = alist;
    
    while (current.is_pair()) {
        auto pair = current.as_pair();
        Object key = pair->car;
        Object rest = pair->cdr;
        
        if (rest.is_pair()) {
            auto value_pair = rest.as_pair();
            Object value = value_pair->car;
            
            if (key.is_symbol() || key.is_string()) {
                field_values[key.to_std_string()] = value;
            }
        }
        current = pair->cdr;
    }
    
    // Заполняем поля структуры
    for (const auto& field : type->fields()) {
        auto it = field_values.find(field.name());
        if (it != field_values.end()) {
            Type* field_type = get_field_type(ts, field);
            if (field_type) {
                // ЗАПИСЫВАЕМ значение поля и получаем размер
                size_t field_written = write_from_data(ts, dest, 
                    offset + field.offset(), field_type, it->second);
                
                // Проверяем что записалось правильно
                if (field_written > 0) {
                    total_written = std::max(total_written, 
                        field.offset() + field_written);
                }
            }
        }
    }
    
    return total_written;
}

// ========================================================================
// Запись enum типов
// ========================================================================

size_t StaticBufferUtils::write_enum_data(TypeSystem* ts, StaticBuffer* dest,
                                       size_t offset, EnumType* type, const Object& source) {
    if (!type) return 0;
    
    size_t enum_size = type->get_size_in_memory();
    check_bounds(dest, offset, enum_size, "write_enum", type->get_name());

    int64_t enum_value = 0;
    
    if (source.is_symbol() || source.is_string()) {
        // Поиск по имени
        std::string name = source.to_std_string();
        const auto& entries = type->entries();
        
        auto it = entries.find(name);
        if (it != entries.end()) {
            enum_value = it->second;
        } else {
            throw std::runtime_error("Unknown enum value: " + name);
        }
    }
    else if (source.is_integer()) {
        // Прямое числовое значение
        enum_value = source.as_integer();
    }
    else {
        throw std::runtime_error("Invalid source for enum");
    }
    
    // Записываем как обычное значение
    write_value_data(ts, dest, offset, type, Object::make_integer(enum_value));

    return enum_size;
}

// ========================================================================
// Запись битовых полей
// ========================================================================

size_t StaticBufferUtils::write_bitfield_data(TypeSystem* ts, StaticBuffer* dest,
                                           size_t offset, BitFieldType* type, const Object& source) {
    if (!type) return 0;
    
    size_t bitfield_size = type->get_size_in_memory();
    check_bounds(dest, offset, bitfield_size, "write_bitfield", type->get_name());

    uint32_t bitfield_value = 0;
    
    if (source.is_integer()) {
        // Прямое числовое значение
        bitfield_value = static_cast<uint32_t>(source.as_integer());
    }
    else if (source.is_list()) {
        // Список флагов
        Object current = source;
        while (current.is_pair()) {
            auto pair = current.as_pair();
            Object item = pair->car;
            
            if (item.is_symbol() || item.is_string()) {
                std::string flag_name = item.to_std_string();
                
                // Ищем флаг в полях битового типа
                for (const auto& field : type->fields()) {
                    if (field.name() == flag_name) {
                        int bit_offset = field.offset();
                        int bit_size = field.size();
                        uint32_t mask = ((1u << bit_size) - 1) << bit_offset;
                        bitfield_value |= mask;
                        break;
                    }
                }
            }
            current = pair->cdr;
        }
    }
    else {
        throw std::runtime_error("Invalid source for bitfield");
    }
    
    // Записываем как обычное значение
    write_value_data(ts, dest, offset, type, Object::make_integer(bitfield_value));

    return bitfield_size;
}

// ========================================================================
// Основные методы чтения (ТО, ЧТО УЖЕ ЕСТЬ У ТЕБЯ)
// ========================================================================

Object StaticBufferUtils::read_value(TypeSystem* ts, StaticBuffer* src,
                                    size_t offset, Type* type) {
    if (!ts || !src || !type) {
        return Object::make_null();
    }
    
    // Проверка границ
    size_t size = type->get_size_in_memory();
    try {
        check_bounds(src, offset, size, "read_value", type->get_name());
    }
    catch (const std::exception&) {
        return Object::make_null();
    }
    
    // Маршрутизация по категориям типов
    std::string class_name = type->get_class_name();
    
    if (class_name == "value") {
        auto* value_type = dynamic_cast<ValueType*>(type);
        if (value_type) {
            return read_primitive_value(ts, src, offset, value_type);
        }
    }
    else if (class_name == "structure" || class_name == "basic") {
        auto* struct_type = dynamic_cast<StructureType*>(type);
        if (struct_type) {
            return read_structure(ts, src, offset, struct_type);
        }
    }
    else if (class_name == "enum") {
        auto* enum_type = dynamic_cast<EnumType*>(type);
        if (enum_type) {
            return read_enum_value(ts, src, offset, enum_type);
        }
    }
    else if (class_name == "bitfield") {
        auto* bitfield_type = dynamic_cast<BitFieldType*>(type);
        if (bitfield_type) {
            return read_bitfield_value(ts, src, offset, bitfield_type);
        }
    }
    else if (type->get_name() == "string" || type->get_name() == "symbol") {
        return read_string_value(ts, src, offset, type);
    }
    
    return Object::make_null();
}

Object StaticBufferUtils::read_field(TypeSystem* ts, StaticBuffer* src,
                                    size_t base_offset, Type* type,
                                    const std::string& field_name) {
    if (!ts || !src || !type) {
        return Object::make_null();
    }
    
    auto* struct_type = dynamic_cast<StructureType*>(type);
    if (!struct_type) {
        return Object::make_null();
    }
    
    // Поиск поля
    Field field;
    if (!struct_type->lookup_field(field_name, &field)) {
        return Object::make_null();
    }
    
    // Чтение значения поля
    size_t field_offset = base_offset + field.offset();
    Type* field_type = get_field_type(ts, field);
    if (!field_type) {
        return Object::make_null();
    }
    
    return read_value(ts, src, field_offset, field_type);
}

Object StaticBufferUtils::read_structure(TypeSystem* ts, StaticBuffer* src,
                                        size_t offset, StructureType* type) {
    if (!type) {
        return Object::make_null();
    }
    
    // Создаем ассоциативный список для результата
    Object result = Object::make_null();
    
    // Проходим по полям в обратном порядке для построения списка
    for (auto it = type->fields().rbegin(); it != type->fields().rend(); ++it) {
        const auto& field = *it;
        
        // Читаем значение поля
        Object field_value = read_field(ts, src, offset, type, field.name());
        if (field_value.is_null()) {
            continue;
        }
        
        // Создаем пару (field_name . field_value)
        Object field_name_obj = Object::make_symbol(field.name());
        Object pair = Object::make_pair(field_name_obj, field_value);
        
        // Добавляем в список
        if (result.is_null()) {
            result = Object::make_pair(pair, Object::make_null());
        } else {
            result = Object::make_pair(pair, result);
        }
    }
    
    return result;
}

Object StaticBufferUtils::buffer_to_object(TypeSystem* ts, StaticBuffer* src,
                                          size_t offset, Type* type) {
    return read_value(ts, src, offset, type);
}

// ========================================================================
// Чтение примитивных типов (ТО, ЧТО УЖЕ ЕСТЬ У ТЕБЯ)
// ========================================================================

Object StaticBufferUtils::read_primitive_value(TypeSystem* ts, StaticBuffer* src,
                                              size_t offset, ValueType* type) {
    int size = type->get_load_size();
    bool is_signed = type->get_load_signed();
    
    switch (size) {
        case 1: {
            uint8_t value = src->read_u8(offset);
            if (is_signed) {
                return Object::make_integer(static_cast<int8_t>(value));
            } else {
                return Object::make_integer(value);
            }
        }
        
        case 2: {
            uint16_t value = src->read_u16(offset);
            if (is_signed) {
                return Object::make_integer(static_cast<int16_t>(value));
            } else {
                return Object::make_integer(value);
            }
        }
        
        case 4: {
            uint32_t value = src->read_u32(offset);
            
            // Проверяем, не является ли это float
            if (type->get_name() == "float") {
                float float_value;
                std::memcpy(&float_value, &value, 4);
                return Object::make_float(float_value);
            }
            
            if (is_signed) {
                return Object::make_integer(static_cast<int32_t>(value));
            } else {
                return Object::make_integer(value);
            }
        }
        
        case 8: {
            uint64_t value = src->read_u64(offset);
            
            // Проверяем, не является ли это double
            if (type->get_name() == "double") {
                double double_value;
                std::memcpy(&double_value, &value, 8);
                return Object::make_float(double_value);
            }
            
            if (is_signed) {
                return Object::make_integer(static_cast<int64_t>(value));
            } else {
                return Object::make_integer(static_cast<int64_t>(value));
            }
        }
        
        default:
            return Object::make_null();
    }
}

// ========================================================================
// Чтение enum типов (ТО, ЧТО УЖЕ ЕСТЬ У ТЕБЯ)
// ========================================================================

Object StaticBufferUtils::read_enum_value(TypeSystem* ts, StaticBuffer* src,
                                         size_t offset, EnumType* type) {
    // Сначала читаем как обычное значение
    Object numeric_value = read_primitive_value(ts, src, offset, type);
    if (!numeric_value.is_integer()) {
        return numeric_value;
    }
    
    int64_t value = numeric_value.as_integer();
    
    // Пытаемся найти символьное имя
    const auto& entries = type->entries();
    for (const auto& entry : entries) {
        if (entry.second == value) {
            return Object::make_symbol(entry.first);
        }
    }
    
    // Если не нашли имя, возвращаем числовое значение
    return numeric_value;
}

// ========================================================================
// Чтение битовых полей (ТО, ЧТО УЖЕ ЕСТЬ У ТЕБЯ)
// ========================================================================

Object StaticBufferUtils::read_bitfield_value(TypeSystem* ts, StaticBuffer* src,
                                             size_t offset, BitFieldType* type) {
    // Сначала читаем как обычное значение
    Object numeric_value = read_primitive_value(ts, src, offset, type);
    if (!numeric_value.is_integer()) {
        return numeric_value;
    }
    
    uint32_t value = static_cast<uint32_t>(numeric_value.as_integer());
    
    // Если тип является битовым полем, создаем список флагов
    Object result_list = Object::make_null();
    const auto& fields = type->fields();
    
    // Проверяем каждый возможный флаг
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        const auto& field = *it;
        
        // Получаем маску для этого поля
        uint32_t mask = 0;
        int bit_offset = field.offset();
        int bit_size = field.size();
        
        if (bit_size > 0 && bit_offset >= 0) {
            mask = ((1u << bit_size) - 1) << bit_offset;
            
            // Проверяем, установлен ли этот флаг
            if ((value & mask) == mask) {
                Object flag_symbol = Object::make_symbol(field.name());
                if (result_list.is_null()) {
                    result_list = Object::make_pair(flag_symbol, Object::make_null());
                } else {
                    result_list = Object::make_pair(flag_symbol, result_list);
                }
            }
        }
    }
    
    // Если нет установленных флагов, возвращаем числовое значение
    if (result_list.is_null()) {
        return numeric_value;
    }
    
    return result_list;
}

// ========================================================================
// Чтение строк и символов (ТО, ЧТО УЖЕ ЕСТЬ У ТЕБЯ)
// ========================================================================

Object StaticBufferUtils::read_string_value(TypeSystem* ts, StaticBuffer* src,
                                           size_t offset, Type* type) {
    // Проверяем границы для минимальной безопасности
    if (offset >= src->size()) {
        return Object::make_string("");
    }
    
    // Ищем нулевой терминатор
    size_t max_len = src->size() - offset;
    size_t len = 0;
    
    while (len < max_len && src->data()[offset + len] != 0) {
        len++;
    }
    
    if (len == 0) {
        return Object::make_string("");
    }
    
    // Создаем строку
    std::string str(reinterpret_cast<const char*>(src->data() + offset), len);
    
    if (type->get_name() == "symbol") {
        return Object::make_symbol(str);
    } else {
        return Object::make_string(str);
    }
}

// ========================================================================
// Утилиты конвертации и сравнения (ТО, ЧТО УЖЕ ЕСТЬ У ТЕБЯ)
// ========================================================================

void StaticBufferUtils::copy_with_conversion(TypeSystem* ts,
                                            StaticBuffer* dest, size_t dest_offset, Type* dest_type,
                                            StaticBuffer* src, size_t src_offset, Type* src_type) {
    // 1. Если типы идентичны - простое копирование
    if (is_compatible_types(ts, src_type, dest_type) &&
        src_type->get_size_in_memory() == dest_type->get_size_in_memory()) {
        size_t size = src_type->get_size_in_memory();
        std::memcpy(dest->data() + dest_offset, src->data() + src_offset, size);
        return;
    }
    
    // 2. Чтение из источника и запись в приемник
    Object value = read_value(ts, src, src_offset, src_type);
    write_from_data(ts, dest, dest_offset, dest_type, value);
}

bool StaticBufferUtils::compare_data(TypeSystem* ts,
                                    StaticBuffer* buf1, size_t offset1, Type* type1,
                                    StaticBuffer* buf2, size_t offset2, Type* type2) {
    // 1. Простое сравнение байтов для совместимых типов
    if (is_compatible_types(ts, type1, type2) &&
        type1->get_size_in_memory() == type2->get_size_in_memory()) {
        size_t size = type1->get_size_in_memory();
        return std::memcmp(buf1->data() + offset1, 
                          buf2->data() + offset2, 
                          size) == 0;
    }
    
    // 2. Сравнение через значения
    Object val1 = read_value(ts, buf1, offset1, type1);
    Object val2 = read_value(ts, buf2, offset2, type2);
    return val1 == val2;
}

// ========================================================================
// Отладочные функции (ТО, ЧТО УЖЕ ЕСТЬ У ТЕБЯ)
// ========================================================================

std::string StaticBufferUtils::dump_value(TypeSystem* ts, StaticBuffer* src,
                                         size_t offset, Type* type, int indent) {
    std::string indent_str(indent, ' ');
    
    if (!type) {
        return indent_str + "<null-type>";
    }
    
    try {
        Object value = read_value(ts, src, offset, type);
        
        std::string result = indent_str + fmt::format("{}: ", type->get_name());
        
        if (value.is_integer()) {
            result += fmt::format("{} (0x{:x})", 
                                 value.as_integer(),
                                 static_cast<uint64_t>(value.as_integer()));
        }
        else if (value.is_float()) {
            result += fmt::format("{}", value.as_float());
        }
        else if (value.is_string()) {
            result += fmt::format("\"{}\"", value.as_string()->data);
        }
        else if (value.is_symbol()) {
            result += fmt::format("'{}", value.as_string()->data);
        }
        else if (value.is_pair()) {
            result += "alist";
        }
        else if (value.is_null()) {
            result += "null";
        }
        else {
            result += value.print();
        }
        
        return result;
    }
    catch (const std::exception& e) {
        return indent_str + fmt::format("{}: ERROR - {}", 
                                       type->get_name(), e.what());
    }
}

// ========================================================================
// Отладочные функции структуры
// ========================================================================

std::string StaticBufferUtils::dump_structure(TypeSystem* ts, StaticBuffer* src,
                                             size_t offset, StructureType* type) {
    std::string result;
    
    if (!type) {
        return "<null-structure>";
    }
    
    result += fmt::format("Structure {}:\n", type->get_name());
    
    for (const auto& field : type->fields()) {
        Object field_value = read_field(ts, src, offset, type, field.name());
        result += fmt::format("  {}: {}\n", field.name(), field_value.print());
    }
    
    return result;
}

} // namespace script
