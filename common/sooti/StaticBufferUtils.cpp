#include "StaticBufferUtils.hpp"

namespace script {

// ========================================================================
// Основной публичный интерфейс
// ========================================================================

void StaticBufferUtils::write_recursive(TypeSystem* ts, StaticBuffer* dest,
                                       size_t offset, Type* type, const Object& source) {
    // 1. Проверка аргументов
    if (!ts || !dest || !type) {
        throw std::runtime_error("[StaticBuffer] Invalid arguments");
    }
    
    // 2. Инициализация памяти под тип
    write_from_type(ts, dest, offset, type);
    
    // 3. Если источник null - возвращаем (оставляем значения по умолчанию)
    if (source.is_null()) {
        return;
    }
    
    // 4. Определяем тип источника и вызываем соответствующий метод
    if (source.is_native_ref()) {
        auto src_buf = source.as_native_ref<StaticBuffer>();
        if (src_buf) {
            write_from_buffer(ts, dest, offset, type, src_buf.get());
        }
    } else {
        write_from_data(ts, dest, offset, type, source);
    }
}

void StaticBufferUtils::write_from_type(TypeSystem* ts, StaticBuffer* dest,
                                       size_t offset, Type* type) {
    // 1. Проверка границ
    size_t size = type->get_size_in_memory();
    check_bounds(dest, offset, size, "write_from_type", type->get_name());
    
    // 2. Базовое зануление
    std::memset(dest->data() + offset, 0, size);
    
    // 3. Специфическая инициализация по категории типа
    std::string class_name = type->get_class_name();
    
    if (class_name == "basic") {
        initialize_basic_type(ts, dest, offset, type);
    }
    else if (class_name == "structure") {
        auto* struct_type = safe_dynamic_cast<StructureType>(type, "structure");
        initialize_structure_type(ts, dest, offset, struct_type);
    }
    // Для value, enum, bitfield типов достаточно зануления
}

void StaticBufferUtils::write_from_buffer(TypeSystem* ts, StaticBuffer* dest,
                                         size_t offset, Type* type, StaticBuffer* src) {
    // 1. Проверка аргументов
    if (!src) {
        throw std::runtime_error("[StaticBuffer] Source buffer is null");
    }
    
    // 2. Проверка границ
    size_t size = type->get_size_in_memory();
    check_bounds(dest, offset, size, "write_from_buffer", type->get_name());
    
    if (src->size() < size) {
        throw std::runtime_error(fmt::format(
            "[StaticBuffer] Source buffer too small: {} < {}",
            src->size(), size
        ));
    }
    
    // 3. Копирование данных
    std::memcpy(dest->data() + offset, src->data(), size);
    
    // 4. Копирование релокаций
    copy_relocations(dest, src, offset);
    
    // 5. Исправление тип-тега для basic типов
    if (type->get_class_name() == "basic") {
        fix_type_tag(ts, dest, offset, type);
    }
}

void StaticBufferUtils::write_from_data(TypeSystem* ts, StaticBuffer* dest,
                                       size_t offset, Type* type, const Object& source) {
    // 1. Для null источника просто инициализируем тип
    if (source.is_null()) {
        write_from_type(ts, dest, offset, type);
        return;
    }
    
    // 2. Маршрутизация по категориям типов
    std::string class_name = type->get_class_name();
    
    if (class_name == "value") {
        auto* value_type = safe_dynamic_cast<ValueType>(type, "value");
        write_value_data(ts, dest, offset, value_type, source);
    }
    else if (class_name == "structure" || class_name == "basic") {
        auto* struct_type = safe_dynamic_cast<StructureType>(type, class_name);
        write_structure_data(ts, dest, offset, struct_type, source);
    }
    else if (class_name == "enum") {
        auto* enum_type = safe_dynamic_cast<EnumType>(type, "enum");
        write_enum_data(ts, dest, offset, enum_type, source);
    }
    else if (class_name == "bitfield") {
        auto* bitfield_type = safe_dynamic_cast<BitFieldType>(type, "bitfield");
        write_bitfield_data(ts, dest, offset, bitfield_type, source);
    }
    else if (type->get_name() == "string" || type->get_name() == "symbol") {
        write_string_data(ts, dest, offset, type, source);
    }
    else {
        throw std::runtime_error(fmt::format(
            "[StaticBuffer] Unsupported type: {}", type->get_name()
        ));
    }
}

// ========================================================================
// Вспомогательные методы для write_from_type
// ========================================================================

void StaticBufferUtils::initialize_basic_type(TypeSystem* ts, StaticBuffer* dest,
                                             size_t offset, Type* type) {
    auto& config = ts->get_config();
    
    // Записываем CRC тип-тег
    uint32_t type_tag = type->get_type_tag();
    dest->write_u32(offset, type_tag);
    
    // Для некоторых basic типов есть heap_base
    if (type->heap_base() != 0 && offset + 8 <= dest->size()) {
        dest->write_u32(offset + 4, type->heap_base());
    }
}

void StaticBufferUtils::initialize_structure_type(TypeSystem* ts, StaticBuffer* dest,
                                                 size_t offset, StructureType* type) {
    for (const auto& field : type->fields()) {
        process_structure_field(ts, dest, offset, field);
    }
}

void StaticBufferUtils::process_structure_field(TypeSystem* ts, StaticBuffer* dest,
                                              size_t base_offset, const Field& field) {
    size_t field_offset = base_offset + field.offset();
    
    if (field.is_inline()) {
        // Рекурсивная инициализация inline структур
        Type* field_type = get_field_type(ts, field);
        if (field_type) {
            write_from_type(ts, dest, field_offset, field_type);
        }
    }
    else if (field.is_array()) {
        // Инициализация массива
        initialize_array_field(ts, dest, field_offset, field);
    }
    else if (field.is_dynamic()) {
        // Динамические поля = nullptr
        dest->write_u32(field_offset, 0);
    }
    // Обычные поля уже занулены
}

// ========================================================================
// Вспомогательные методы для write_from_data
// ========================================================================

void StaticBufferUtils::write_value_data(TypeSystem* ts, StaticBuffer* dest,
                                        size_t offset, ValueType* type, const Object& source) {
    int size = type->get_load_size();
    
    if (source.is_integer()) {
        int64_t value = source.as_integer();
        
        if (size == 1) dest->write_u8(offset, static_cast<uint8_t>(value));
        else if (size == 2) dest->write_u16(offset, static_cast<uint16_t>(value));
        else if (size == 4) dest->write_u32(offset, static_cast<uint32_t>(value));
        else if (size == 8) dest->write_u64(offset, static_cast<uint64_t>(value));
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
            dest->write_u32(offset, static_cast<uint32_t>(bits));
            dest->write_u32(offset + 4, static_cast<uint32_t>(bits >> 32));
        }
    }
    else {
        throw std::runtime_error(fmt::format(
            "[StaticBuffer] Expected number for value type '{}'",
            type->get_name()
        ));
    }
}

void StaticBufferUtils::write_structure_data(TypeSystem* ts, StaticBuffer* dest,
                                            size_t offset, StructureType* type, const Object& source) {
    // 1. Базовая инициализация
    write_from_type(ts, dest, offset, type);
    
    // 2. Определяем формат источника
    if (source.is_list()) {
        write_from_alist(ts, dest, offset, type, source);
    }
    else if (source.is_hash_table()) {
        write_from_hash_table(ts, dest, offset, type, source);
    }
    else {
        throw std::runtime_error(fmt::format(
            "[StaticBuffer] Unsupported source format for structure '{}'",
            type->get_name()
        ));
    }
}

void StaticBufferUtils::write_from_alist(TypeSystem* ts, StaticBuffer* dest,
                                        size_t offset, StructureType* type, const Object& alist) {
    // Строим map полей из alist
    std::unordered_map<std::string, Object> field_values;
    Object current = alist;
    
    while (current.is_pair()) {
        auto pair = current.as_pair();
        Object item = pair->car;
        if (item.is_pair()) {
            auto item_pair = item.as_pair();
            Object key = item_pair->car;
            Object value = item_pair->cdr;
            
            if (key.is_symbol() || key.is_string()) {
                field_values[key.to_std_string()] = value;
            }
        }
        current = pair->cdr;
    }
    
    // Заполняем поля
    for (const auto& field : type->fields()) {
        auto it = field_values.find(field.name());
        if (it != field_values.end()) {
            Type* field_type = get_field_type(ts, field);
            if (field_type) {
                write_from_data(ts, dest, offset + field.offset(), field_type, it->second);
            }
        }
    }
}

// ========================================================================
// Общие утилиты
// ========================================================================

void StaticBufferUtils::check_bounds(StaticBuffer* dest, size_t offset, size_t size,
                                    const std::string& operation, const std::string& type_name) {
    if (offset + size > dest->size()) {
        throw std::runtime_error(fmt::format(
            "[StaticBuffer] {} overflow for type '{}': offset {} + size {} > buffer size {}",
            operation, type_name, offset, size, dest->size()
        ));
    }
}

Type* StaticBufferUtils::get_field_type(TypeSystem* ts, const Field& field) {
    try {
        return ts->lookup_type(field.type());
    }
    catch (const std::exception& e) {
        throw std::runtime_error(fmt::format(
            "[StaticBuffer] Cannot lookup type for field '{}': {}",
            field.name(), e.what()
        ));
    }
}

// ========================================================================
// Доступ к полям структуры
// ========================================================================

Object StaticBufferUtils::read_field(TypeSystem* ts, StaticBuffer* dest, size_t base_offset, Type* type, const std::string& field_name) {
    if (auto* struct_type = dynamic_cast<StructureType*>(type)) {
        Field field;
        if (struct_type->lookup_field(field_name, &field)) {
            size_t field_offset = base_offset + field.offset();
            Type* field_type = ts->lookup_type(field.type());

            // Вызываем атомарное чтение в зависимости от типа поля
            return read_primitive(ts, dest, field_offset, field_type);
        }
    }
    return Object::make_null();
}

Object StaticBufferUtils::read_primitive(TypeSystem* ts, StaticBuffer* dest, size_t offset, Type* type) {
    int size = type->get_load_size();
    bool is_signed = type->get_load_signed();

    // Используем твои новые методы read_u32_le / read_u64_le
    if (type->get_class_name() == "value") {
        if (size == 4) return Object::make_integer(dest->read_u32_le(offset));
        if (size == 8) return Object::make_integer(dest->read_u64_le(offset));
        // ... и так далее
    }
    if (type->get_name() == "string") {
        // Читаем как строку до нулевого байта
        return Object::make_string(dest->read_string(offset));
    }
    return Object::make_null();
}

} // namespace script