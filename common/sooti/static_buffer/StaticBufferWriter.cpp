#include "StaticBufferWriter.hpp"
// Include the header file for TypeSystem class
#include "common/sooti/Object.hpp"
#include "common/type_system/TypeSystem.hpp"
#include <cstring>

namespace script {

/**
 * Запись значения в указатель.
 * Специальная обработка строк (так как они могут быть "string" или "symbol").
 * В остальных случаях - вызывает write_primitive_at_ptr, write_enum_at_ptr или
 * write_bitfield_at_ptr в зависимости от класса типа.
 */
void StaticBufferWriter::write_value_at_ptr(void *ptr, Type *type, const Object &val) {
    if (auto *val_type = dynamic_cast<ValueType *>(type)) {
        write_primitive_at_ptr(ptr, val_type, val);
    } else if (auto *enum_type = dynamic_cast<EnumType *>(type)) {
        write_enum_at_ptr(ptr, enum_type, val);
    } else if (auto *bf_type = dynamic_cast<BitFieldType *>(type)) {
        write_bitfield_at_ptr(ptr, bf_type, val);
    } else if (type->get_name() == "string") {
        // На самом деле мы пишем в буффер произвольные данные
        // по произвольному адресу тут 'string' просто подсдсказка
        // как и int, float и т.д. btw the strings are BasicType
        write_string_at_ptr(ptr, type, val);
    } else {
        throw std::runtime_error("StaticBufferWriter: Unsupported type for direct write: " +
                                 type->get_name());
    }
}
/**
 * Запись примитивного значения в указатель.
 * Используется для записи значений, тип которых является
 * ValueType (например, int, float, double и т.д.).
 *
 * @param[in] ptr Указатель на место записи
 * @param[in] type Тип, который записывается в указатель
 * @param[in] val Значение, которое записывается в указатель
 */
void StaticBufferWriter::write_primitive_at_ptr(void *ptr, ValueType *type, const Object &val) {
    if (!ptr || !type) {
        throw std::runtime_error("StaticBufferWriter: Null pointer or type");
    }

    int         size = type->get_load_size();
    bool        is_signed = type->get_load_signed();
    std::string type_name = type->get_name();

    // 1. Получаем базовое значение (только числа)
    int64_t i_val = 0;
    double  f_val = 0.0;
    bool    is_num = false;

    if (val.is_integer()) {
        i_val = val.as_integer();
        f_val = static_cast<double>(i_val);
        is_num = true;
    } else if (val.is_float()) {
        f_val = val.as_float();
        i_val = static_cast<int64_t>(f_val);
        is_num = true;
    } else if (val.is_symbol()) {
        // Поддержка булевых констант как 1 и 0
        if (val.to_std_string() == "true" || val.to_std_string() == "#t") {
            i_val = 1;
            f_val = 1.0;
            is_num = true;
        } else if (val.to_std_string() == "false" || val.to_std_string() == "#f") {
            i_val = 0;
            f_val = 0.0;
            is_num = true;
        }
    }

    if (!is_num) {
        throw std::runtime_error(fmt::format(
            "StaticBufferWriter: Type mismatch. Type '{}' expects a number, but got '{}' ({})",
            type_name, val.print(), object_type_to_string(val.type)));
    }

    // 2. Проверка переполнения и физическая запись
    switch (size) {
    case 1: { // 8-bit
        if (is_signed) {
            if (i_val < -128 || i_val > 127)
                throw std::runtime_error(fmt::format(
                    "Overflow: {} does not fit in signed 8-bit ({})", i_val, type_name));
            *(int8_t *)ptr = static_cast<int8_t>(i_val);
        } else {
            if (i_val < 0 || i_val > 255)
                throw std::runtime_error(fmt::format(
                    "Overflow: {} does not fit in unsigned 8-bit ({})", i_val, type_name));
            *(uint8_t *)ptr = static_cast<uint8_t>(i_val);
        }
        break;
    }

    case 2: { // 16-bit
        if (is_signed) {
            if (i_val < -32768 || i_val > 32767)
                throw std::runtime_error(fmt::format(
                    "Overflow: {} does not fit in signed 16-bit ({})", i_val, type_name));
            *(int16_t *)ptr = static_cast<int16_t>(i_val);
        } else {
            if (i_val < 0 || i_val > 65535)
                throw std::runtime_error(fmt::format(
                    "Overflow: {} does not fit in unsigned 16-bit ({})", i_val, type_name));
            *(uint16_t *)ptr = static_cast<uint16_t>(i_val);
        }
        break;
    }

    case 4: { // 32-bit
        if (type_name == "float") {
            *(float *)ptr = static_cast<float>(f_val);
        } else {
            if (is_signed) {
                if (i_val < -2147483648LL || i_val > 2147483647LL)
                    throw std::runtime_error(
                        fmt::format("Overflow: {} does not fit in signed 32-bit", i_val));
                *(int32_t *)ptr = static_cast<int32_t>(i_val);
            } else {
                if (i_val < 0 || i_val > 4294967295LL)
                    throw std::runtime_error(
                        fmt::format("Overflow: {} does not fit in unsigned 32-bit", i_val));
                *(uint32_t *)ptr = static_cast<uint32_t>(i_val);
            }
        }
        break;
    }

    case 8: { // 64-bit
        if (type_name == "double") {
            *(double *)ptr = f_val;
        } else {
            // Для 64-бит при использовании int64_t переполнение по знаку проверить сложно,
            // но проверим хотя бы на отрицательные значения для unsigned.
            if (!is_signed && i_val < 0)
                throw std::runtime_error(
                    fmt::format("Overflow: Negative value {} in unsigned 64-bit", i_val));
            *(uint64_t *)ptr = static_cast<uint64_t>(i_val);
        }
        break;
    }

    default:
        throw std::runtime_error(fmt::format(
            "StaticBufferWriter: Unsupported primitive size {} for type '{}'", size, type_name));
    }
}

/**
 * @brief Запись значения enum в указатель.
 *
 * @param[in] ptr Указатель на место записи
 * @param[in] type Тип, который записывается в указатель
 * @param[in] val Значение, которое записывается в указатель
 *
 * Если val является символом или строкой, то мы ищем соответствующее
 * значение в словаре энума и записываем его в указатель.
 * Если val является целым числом, то мы записываем его в указатель
 * как примитив.
 *
 * @details В остальных случаях (например, если val является списком)
 * ничего не записывается.
 */
void StaticBufferWriter::write_enum_at_ptr(void *ptr, EnumType *type, const Object &val) {
    int64_t numeric_to_write = 0;

    if (val.is_symbol() || val.is_string()) {
        // Ищем число по имени в словаре энума
        std::string name = val.to_std_string();
        const auto &entries = type->entries();
        auto        it = entries.find(name);
        if (it != entries.end()) {
            numeric_to_write = it->second;
        }
    } else if (val.is_integer()) {
        numeric_to_write = val.as_integer();
    } else {
        throw std::runtime_error(
            "StaticBufferWriter: Expected int, float, or symbol for primitive write, got " +
            object_type_to_string(val.type) + " with value " + val.print());
    }

    // Записываем полученное число как примитив
    write_primitive_at_ptr(ptr, type, Object::make_integer(numeric_to_write));
}

/**
 * @brief Запись значения битового поля в указатель.
 *
 * @param[in] ptr Указатель на место записи
 * @param[in] type Тип, который записывается в указатель
 * @param[in] val Значение, которое записывается в указатель
 *
 * Если val является целым числом, то мы записываем его в указатель
 * как примитив.
 * Если val является списком символов-флагов, то мы собираем маску
 * из списка символов-флагов и записываем ее в указатель как примитив.
 *
 * @details В остальных случаях (например, если val является строкой)
 * ничего не записывается.
 */
void StaticBufferWriter::write_bitfield_at_ptr(void *ptr, BitFieldType *type, const Object &val) {
    uint64_t final_mask = 0;

    if (val.is_integer()) {
        final_mask = static_cast<uint64_t>(val.as_integer());
    } else if (val.is_list() || val.is_pair()) {
        // Собираем маску из списка символов-флагов
        Object current = val;
        while (current.is_pair()) {
            Object item = current.as_pair()->car;
            if (item.is_symbol() || item.is_string()) {
                std::string flag_name = item.to_std_string();
                for (const auto &field : type->fields()) {
                    if (field.name() == flag_name) {
                        uint64_t field_mask = ((1ULL << field.size()) - 1) << field.offset();
                        final_mask |= field_mask;
                        break;
                    }
                }
            }
            current = current.as_pair()->cdr;
        }
    } else {
        throw std::runtime_error(
            "StaticBufferWriter: Expected int, float, or symbol for primitive write, got " +
            object_type_to_string(val.type) + " with value " + val.print());
    }

    write_primitive_at_ptr(ptr, type, Object::make_integer(final_mask));
}

/**
 * @brief Запись строки в указатель.
 *
 * Если val является строкой или символом, то мы копируем строку
 * в память, начиная с указателя ptr.
 *
 * @details ВАЖНО: Мы не знаем размер буфера здесь, поэтому предполагаем,
 * что память под строку была заранее подготовлена/аллоцирована.
 */
void StaticBufferWriter::write_string_at_ptr(void *ptr, Type *type, const Object &val) {
    if (!val.is_string() && !val.is_symbol()) {
        throw std::runtime_error(
            "StaticBufferWriter: Expected string or symbol for primitive write, got " +
            object_type_to_string(val.type) + " with value " + val.print());
        return;
    }

    std::string str = val.to_std_string();
    // Просто копируем строку в память.
    // ВАЖНО: Мы не знаем размер буфера здесь, поэтому предполагаем,
    // что память под строку была заранее подготовлена/аллоцирована.
    std::strcpy(reinterpret_cast<char *>(ptr), str.c_str());
}

} // namespace script