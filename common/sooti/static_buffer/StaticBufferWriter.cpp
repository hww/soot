#include "StaticBufferWriter.hpp"
#include <cstring>
#include <stdexcept>

namespace script {

void StaticBufferWriter::write_value_at_ptr(void* ptr, Type* type, const Object& val) {
    if (!ptr || !type) return;
    TypeSystem* ts = &TypeSystem::instance();
    
    // Специальная обработка строк (так как они могут быть "string" или "symbol")
    const std::string& type_name = type->get_name();
    if (type_name == "string" || type_name == "symbol") {
        write_string_at_ptr(ptr, type, val);
        return;
    }

    std::string class_name = type->get_class_name();

    if (class_name == "value") {
        write_primitive_at_ptr(ptr, static_cast<ValueType*>(type), val);
    } 
    else if (class_name == "enum") {
        write_enum_at_ptr(ptr, static_cast<EnumType*>(type), val);
    } 
    else if (class_name == "bitfield") {
        write_bitfield_at_ptr(ptr, static_cast<BitFieldType*>(type), val);
    }
    // Запись структур целиком (через alist) здесь не нужна, 
    // так как TypeCell позволяет писать в каждое поле отдельно.
}

void StaticBufferWriter::write_primitive_at_ptr(void* ptr, ValueType* type, const Object& val) {
    int size = type->get_load_size();
    
    // Пытаемся получить числовое значение из объекта
    int64_t i_val = 0;
    double f_val = 0.0;
    bool is_float_type = (type->get_name() == "float" || type->get_name() == "double");

    if (val.is_integer()) {
        i_val = val.as_integer();
        f_val = static_cast<double>(i_val);
    } else if (val.is_float()) {
        f_val = val.as_float();
        i_val = static_cast<int64_t>(f_val);
    } else {
        return; // Или кинуть ошибку: тип не совпадает
    }

    // Физическая запись
    switch (size) {
        case 1: *(uint8_t*)ptr = static_cast<uint8_t>(i_val); break;
        case 2: *(uint16_t*)ptr = static_cast<uint16_t>(i_val); break;
        case 4: {
            if (type->get_name() == "float") {
                *(float*)ptr = static_cast<float>(f_val);
            } else {
                *(uint32_t*)ptr = static_cast<uint32_t>(i_val);
            }
            break;
        }
        case 8: {
            if (type->get_name() == "double") {
                *(double*)ptr = f_val;
            } else {
                *(uint64_t*)ptr = static_cast<uint64_t>(i_val);
            }
            break;
        }
    }
}

void StaticBufferWriter::write_enum_at_ptr(void* ptr, EnumType* type, const Object& val) {
    int64_t numeric_to_write = 0;

    if (val.is_symbol() || val.is_string()) {
        // Ищем число по имени в словаре энума
        std::string name = val.to_std_string();
        const auto& entries = type->entries();
        auto it = entries.find(name);
        if (it != entries.end()) {
            numeric_to_write = it->second;
        }
    } else if (val.is_integer()) {
        numeric_to_write = val.as_integer();
    }

    // Записываем полученное число как примитив
    write_primitive_at_ptr(ptr, type, Object::make_integer(numeric_to_write));
}

void StaticBufferWriter::write_bitfield_at_ptr(void* ptr, BitFieldType* type, const Object& val) {
    uint64_t final_mask = 0;

    if (val.is_integer()) {
        final_mask = static_cast<uint64_t>(val.as_integer());
    } 
    else if (val.is_list() || val.is_pair()) {
        // Собираем маску из списка символов-флагов
        Object current = val;
        while (current.is_pair()) {
            Object item = current.as_pair()->car;
            if (item.is_symbol() || item.is_string()) {
                std::string flag_name = item.to_std_string();
                for (const auto& field : type->fields()) {
                    if (field.name() == flag_name) {
                        uint64_t field_mask = ((1ULL << field.size()) - 1) << field.offset();
                        final_mask |= field_mask;
                        break;
                    }
                }
            }
            current = current.as_pair()->cdr;
        }
    }

    write_primitive_at_ptr(ptr, type, Object::make_integer(final_mask));
}

void StaticBufferWriter::write_string_at_ptr(void* ptr, Type* type, const Object& val) {
    if (!val.is_string() && !val.is_symbol()) return;
    
    std::string str = val.to_std_string();
    // Просто копируем строку в память. 
    // ВАЖНО: Мы не знаем размер буфера здесь, поэтому предполагаем, 
    // что память под строку была заранее подготовлена/аллоцирована.
    std::strcpy(reinterpret_cast<char*>(ptr), str.c_str());
}

} // namespace script