#include "StaticBufferReader.hpp"
#include "../Object.hpp"

namespace script {

// Универсальный диспетчер чтения по указателю
Object StaticBufferReader::read_value_at_ptr(TypeSystem *ts, void *ptr, Type *type) {
    if (!ptr || !type)
<<<<<<< HEAD
        return Object::make_null();
=======
        throw std::runtime_error("StaticBufferReader invalid poiner");
>>>>>>> temp-branch

    const std::string &name = type->get_name();

    if (name == "string") {
        return read_string_at_ptr(ptr);
    }
    if (name == "symbol") {
        return Object::make_symbol(read_string_at_ptr(ptr).as_string()->data);
    }

<<<<<<< HEAD
    std::string class_name = type->class_name();

    if (class_name == "value") {
        return read_primitive_at_ptr(ptr, static_cast<ValueType *>(type));
    }
    if (class_name == "enum") {
        return read_enum_at_ptr(ts, ptr, static_cast<EnumType *>(type));
    }
    if (class_name == "bitfield") {
=======
    std::string class_name = type->full_class_name();

    if (class_name == "ValueType") {
        return read_primitive_at_ptr(ptr, static_cast<ValueType *>(type));
    }
    if (class_name == "EnumType") {
        return read_enum_at_ptr(ts, ptr, static_cast<EnumType *>(type));
    }
    if (class_name == "BitFieldType") {
>>>>>>> temp-branch
        return read_bitfield_at_ptr(ptr, static_cast<BitFieldType *>(type));
    }

    // Если это структура, мы можем сгенерировать alist "на лету"
    // (например, для отладки или команды inspect)
    if (auto *st = dynamic_cast<StructureType *>(type)) {
        return read_structure_to_alist(ts, ptr, st);
    }

    return Object::make_null();
}

// Чтение примитива
Object StaticBufferReader::read_primitive_at_ptr(void *ptr, ValueType *type) {
    int  size = type->get_load_size();
    bool is_signed = type->get_load_signed();

    switch (size) {
    case 1:
        return is_signed ? Object::make_integer(*(int8_t *)ptr)
                         : Object::make_integer(*(uint8_t *)ptr);
    case 2:
        return is_signed ? Object::make_integer(*(int16_t *)ptr)
                         : Object::make_integer(*(uint16_t *)ptr);
    case 4: {
        if (type->get_name() == "float")
            return Object::make_float(*(float *)ptr);
        return is_signed ? Object::make_integer(*(int32_t *)ptr)
                         : Object::make_integer(*(uint32_t *)ptr);
    }
    case 8: {
        if (type->get_name() == "double")
            return Object::make_float(*(double *)ptr);
        return Object::make_integer(*(int64_t *)ptr);
    }
    default:
        return Object::make_null();
    }
}

// Чтение структуры в alist (для полной деривации объекта)
Object StaticBufferReader::read_structure_to_alist(TypeSystem *ts, void *ptr, StructureType *type) {
    Object      result = Object::make_null();
    const auto &fields = type->fields();

    // Итерируемся с конца, чтобы список в Lisp был в правильном порядке (через cons)
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        void *field_ptr = (uint8_t *)ptr + it->offset();
        Type *field_type = ts->lookup_type(it->type());

        Object val = read_value_at_ptr(ts, field_ptr, field_type);
        result = Object::make_pair(Object::make_pair(Object::make_symbol(it->name()), val), result);
    }
    return result;
}

Object StaticBufferReader::read_enum_at_ptr(TypeSystem *ts, void *ptr, EnumType *type) {
    // 1. Сначала читаем физическое значение (как обычный Integer/ValueType)
    // Enum наследуется от ValueType, поэтому мы знаем его размер и знак
    Object numeric_value = read_primitive_at_ptr(ptr, type);

    if (!numeric_value.is_integer()) {
        return numeric_value;
    }

    int64_t value = numeric_value.as_integer();

    // 2. Ищем, соответствует ли это число какому-либо имени (entry)
    const auto &entries = type->entries();
    for (const auto &entry : entries) {
        if (entry.second == value) {
            // Возвращаем имя энума как символ Lisp
            return Object::make_symbol(entry.first);
        }
    }

    // 3. Если в определении типа такого числа нет, возвращаем просто число
    return numeric_value;
}

Object StaticBufferReader::read_bitfield_at_ptr(void *ptr, BitFieldType *type) {
    // 1. Читаем базовое числовое значение битфилда
    Object numeric_value = read_primitive_at_ptr(ptr, type);
    if (!numeric_value.is_integer()) {
        return numeric_value;
    }

    uint64_t    raw_value = static_cast<uint64_t>(numeric_value.as_integer());
    Object      result_list = Object::make_null();
    const auto &fields = type->fields();

    // 2. Проходим по всем определенным битовым полям (в обратном порядке для корректного cons)
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        const auto &field = *it;

        // Создаем маску: берем биты в количестве field.size() со смещением field.offset()
        uint64_t mask = ((1ULL << field.size()) - 1) << field.offset();

        // Проверяем: если это одиночный бит или поле полностью совпадает с маской
        if ((raw_value & mask) != 0) {
            // В зависимости от логики, мы можем возвращать либо список имен установленных флагов,
            // либо, если поле больше 1 бита, значение этого под-поля.
            // Для простоты здесь: возвращаем символ имени, если биты установлены.
            Object flag_symbol = Object::make_symbol(field.name());
            result_list = Object::make_pair(flag_symbol, result_list);
        }
    }

    // 3. Если ни один флаг не распознан, отдаем сырое число
    if (result_list.is_null()) {
        return numeric_value;
    }

    return result_list;
}

Object StaticBufferReader::read_string_at_ptr(void *ptr) {
    if (!ptr)
        return Object::make_null();

    // В GOAL строки обычно - это либо указатель на данные,
    // либо (если это inline) сами данные.
    // Предполагаем, что ptr указывает на начало C-строки.
    const char *str = reinterpret_cast<const char *>(ptr);
    return Object::make_string(str);
}

} // namespace script