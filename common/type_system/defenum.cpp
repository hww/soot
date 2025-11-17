#include "defenum.h"
#include "common/util/bit_utils.h"
#include "common/util/string_util.h"
#include "fmt/format.h"

namespace {

    // Вспомогательные функции для работы с Object
    const script::Object& car(const script::Object* x) {
        if (!x->is_pair()) 
            throw std::runtime_error("invalid defenum form");
        return x->as_pair()->car;
    }

    const script::Object* cdr(const script::Object* x) {
        if (!x->is_pair()) 
            throw std::runtime_error("invalid defenum form");
        return &x->as_pair()->cdr;
    }

    std::string symbol_string(const script::Object& obj) {
        if (obj.is_symbol()) return obj.as_symbol().name_ptr;
        throw std::runtime_error(obj.print() + " was supposed to be a symbol, but isn't");
    }

    int64_t get_int(const script::Object& obj) {
        if (obj.is_integer()) return obj.as_integer();
        throw std::runtime_error(obj.print() + " was supposed to be an integer, but isn't");
    }

    bool is_type(const std::string& expected, const TypeSpec& actual, TypeSystem* ts) {
        TypeSpec* expected_spec = type_system_make_typespec(ts, expected.c_str());
        bool result = type_system_typecheck(ts, expected_spec, &actual);
        type_spec_destroy(expected_spec);
        return result;
    }

    // ФИКС: Убрал взятие адреса временного объекта
    TypeSpec* parse_typespec(TypeSystem* ts, const script::Object& src) {
        if (src.is_symbol()) {
            return type_system_make_typespec(ts, symbol_string(src).c_str());
        }
        else if (src.is_pair()) {
            TypeSpec* tspec = type_system_make_typespec(ts, symbol_string(car(&src)).c_str());
            const auto* rest = cdr(&src);

            while (rest->is_pair()) {
                auto& it = rest->as_pair()->car;
                TypeSpec* arg_spec = parse_typespec(ts, it); // Уже возвращает указатель
                type_spec_add_arg(tspec, arg_spec);
                rest = &rest->as_pair()->cdr;
            }
            return tspec;
        }
        else {
            throw std::runtime_error("invalid typespec: " + src.print());
        }
    }

    // ФИКС: Убрал конфликт имен - переименовал функцию
    bool integer_fits_in_type(int64_t value, int size, bool is_signed) {
        if (is_signed) {
            switch (size) {
            case 1: return value >= -128 && value <= 127;
            case 2: return value >= -32768 && value <= 32767;
            case 4: return value >= -2147483648LL && value <= 2147483647LL;
            case 8: return true;
            default: return false;
            }
        }
        else {
            if (value < 0) return false;
            switch (size) {
            case 1: return value <= 255;
            case 2: return value <= 65535;
            case 4: return value <= 4294967295LL;
            case 8: return value <= 18446744073709551615ULL;
            default: return false;
            }
        }
    }

    // В реальной реализации эти функции должны быть в type_system
    EnumType* try_enum_lookup(TypeSystem* ts, TypeSpec* spec) {
        Type* type = type_system_lookup(ts, spec->base_type);
        type_spec_destroy(spec);
        return type ? (EnumType*)type : nullptr;
    }

    ValueType* get_type_of_type(TypeSystem* ts, const char* type_name) {
        Type* type = type_system_lookup(ts, type_name);
        return type ? (ValueType*)type : nullptr;
    }

} // namespace

EnumType* parse_defenum(const script::Object& defenum,
    TypeSystem* ts,
    DefinitionMetadata* symbol_metadata) {
    // Базовая валидация - defenum должен быть списком (defenum name ...)
    if (!defenum.is_pair()) throw std::runtime_error("defenum must be list");

    // Пропускаем символ 'defenum' и берем имя
    const auto* iter = &defenum;
    if (!iter->is_pair() || !iter->as_pair()->car.is_symbol() ||
        std::string(iter->as_pair()->car.as_symbol().name_ptr) != "defenum") {
        throw std::runtime_error("defenum must start with 'defenum' symbol");
    }

    iter = &iter->as_pair()->cdr; // переходим к имени

    // Получаем имя enum
    if (!iter->is_pair() || !iter->as_pair()->car.is_symbol()) {
        throw std::runtime_error("defenum name must be a symbol");
    }
    std::string name = iter->as_pair()->car.as_symbol().name_ptr;
    fmt::print("DEBUG: Enum name: '{}'\n", name);

    iter = &iter->as_pair()->cdr; // переходим к опциям/entries

    TypeSpec* base_type = type_system_make_typespec(ts, "int64");
    bool is_bitfield = false;
    std::unordered_map<std::string, int64_t> entries;

    // Пропускаем docstring если есть
    if (iter->is_pair() && iter->as_pair()->car.is_string()) {
        if (symbol_metadata) {
            symbol_metadata->docstring = iter->as_pair()->car.as_string();
        }
        iter = &iter->as_pair()->cdr;
    }

    // ОБЪЯВЛЯЕМ base_type_obj ЗДЕСЬ
    Type* base_type_obj = type_system_lookup(ts, base_type->base_type);
    if (!base_type_obj) {
        type_spec_destroy(base_type);
        throw std::runtime_error("Base type not found: " + std::string(base_type->base_type));
    }
    // Парсим опции
    while (iter->is_pair() && iter->as_pair()->car.is_symbol()) {
        const auto& option = iter->as_pair()->car;
        std::string opt_name = option.as_symbol().name_ptr;

        // ПРОВЕРЯЕМ ТОЛЬКО ИЗВЕСТНЫЕ ОПЦИИ
        if (opt_name == ":bitfield" || opt_name == ":type" || opt_name == ":copy-entries") {
            iter = &iter->as_pair()->cdr;
            if (!iter->is_pair()) {
                type_spec_destroy(base_type);
                throw std::runtime_error("Option " + opt_name + " needs value");
            }

            const auto& value = iter->as_pair()->car;

            if (opt_name == ":bitfield") {
                if (!value.is_symbol()) {
                    type_spec_destroy(base_type);
                    throw std::runtime_error(":bitfield value must be symbol");
                }

                std::string bitfield_val = value.as_symbol().name_ptr;
                if (bitfield_val != "#t" && bitfield_val != "#f") {
                    type_spec_destroy(base_type);
                    throw std::runtime_error(":bitfield value must be #t or #f, got: " + bitfield_val);
                }

                is_bitfield = (bitfield_val == "#t");
            }
            else if (opt_name == ":type") {
                TypeSpec* old_base_type = base_type; // сохраняем старый

                if (value.is_symbol()) {
                    base_type = type_system_make_typespec(ts, value.as_symbol().name_ptr);
                    if (!base_type) {
                        // Если не удалось создать typespec, восстанавливаем старый
                        base_type = old_base_type;
                        throw std::runtime_error("Failed to create typespec for: " + std::string(value.as_symbol().name_ptr));
                    }
                    type_spec_destroy(old_base_type); // освобождаем старый только после успеха
                }
                else {
                    throw std::runtime_error("Complex typespec not yet supported");
                }

                // ОБНОВЛЯЕМ base_type_obj после изменения типа
                base_type_obj = type_system_lookup(ts, base_type->base_type);
                if (!base_type_obj) {
                    throw std::runtime_error("Base type not found: " + std::string(base_type->base_type));
                }
            }
            // TODO: :copy-entries

            iter = &iter->as_pair()->cdr;
        }
        else {
            // ЕСЛИ это НЕ известная опция - значит это entry, выходим из цикла опций
            break;
        }
    }


    // Отладочный вывод
    fmt::print("DEBUG: After options parsing, remaining items:\n");
    const auto* debug_iter = iter;
    int count = 0;
    while (debug_iter->is_pair()) {
        fmt::print("  [{}] {}\n", count, debug_iter->as_pair()->car.print());
        debug_iter = &debug_iter->as_pair()->cdr;
        count++;
    }

    // Парсим entries - УПРОЩЕННАЯ ЛОГИКА
    int64_t highest = -1;
    while (iter->is_pair()) {
        auto& field = iter->as_pair()->car;

        if (field.is_symbol()) {
            std::string entry_name = field.as_symbol().name_ptr;

            // Проверяем следующий элемент - может быть значение?
            const auto* next_iter = &iter->as_pair()->cdr;
            if (next_iter->is_pair() && next_iter->as_pair()->car.is_integer()) {
                // Формат: :name value
                int64_t entry_val = next_iter->as_pair()->car.as_integer();

                if (entries.find(entry_name) != entries.end()) {
                    type_spec_destroy(base_type);
                    throw std::runtime_error("Entry " + entry_name + " appears multiple times");
                }

                if (!integer_fits_in_type(entry_val, base_type_obj->get_load_size(base_type_obj),
                    base_type_obj->get_load_signed(base_type_obj))) {
                    fmt::print("Warning: Integer {} does not fit in {}\n", entry_val, base_type_obj->name);
                }

                if (entries.empty()) highest = entry_val;
                highest = std::max(highest, entry_val);
                entries[entry_name] = entry_val;
                fmt::print("DEBUG: Added entry {} = {} (sequential)\n", entry_name, entry_val);

                iter = &next_iter->as_pair()->cdr; // пропускаем значение
            }
            else {
                // Простой символ - авто-инкремент
                if (entries.find(entry_name) != entries.end()) {
                    type_spec_destroy(base_type);
                    throw std::runtime_error("Entry " + entry_name + " appears multiple times");
                }

                entries[entry_name] = ++highest;
                fmt::print("DEBUG: Added entry {} = {} (auto)\n", entry_name, highest);
                iter = &iter->as_pair()->cdr;
            }
        }
        else if (field.is_pair()) {
            // Пара (name value)
            auto& entry_name_obj = field.as_pair()->car;
            auto& value_rest = field.as_pair()->cdr;

            if (!entry_name_obj.is_symbol()) {
                type_spec_destroy(base_type);
                throw std::runtime_error("Enum entry name must be symbol");
            }

            std::string entry_name = entry_name_obj.as_symbol().name_ptr;

            if (entries.find(entry_name) != entries.end()) {
                type_spec_destroy(base_type);
                throw std::runtime_error("Entry " + entry_name + " appears multiple times");
            }

            if (value_rest.is_pair()) {
                // Есть значение: (name value)
                auto& value_obj = value_rest.as_pair()->car;

                // ПРОВЕРЯЕМ что значение - integer
                if (!value_obj.is_integer()) {
                    type_spec_destroy(base_type);
                    throw std::runtime_error("Expected integer for enum value, got: " + value_obj.print());
                }

                int64_t entry_val = value_obj.as_integer();
                if (!integer_fits_in_type(entry_val, base_type_obj->get_load_size(base_type_obj),
                    base_type_obj->get_load_signed(base_type_obj))) {
                    fmt::print("Warning: Integer {} does not fit in {}\n", entry_val, base_type_obj->name);
                }

                if (entries.empty()) highest = entry_val;
                highest = std::max(highest, entry_val);
                entries[entry_name] = entry_val;
                fmt::print("DEBUG: Added entry {} = {} (explicit)\n", entry_name, entry_val);
            }
            else {
                // Авто-инкремент: (name)
                entries[entry_name] = ++highest;
                fmt::print("DEBUG: Added entry {} = {} (auto pair)\n", entry_name, highest);
            }

            iter = &iter->as_pair()->cdr;
        }
        else if (field.is_integer()) {
            // Число как entry - это ошибка
            type_spec_destroy(base_type);
            throw std::runtime_error("Enum entry cannot be a number: " + field.print());
        }
        else {
            type_spec_destroy(base_type);
            throw std::runtime_error("Enum entry must be symbol or list, got: " + field.print());
        }
    }


    // Проверяем что базовый тип - integer
    bool is_integer_type = false;
    Type* current = base_type_obj;
    while (current) {
        if (strcmp(current->name, "integer") == 0 ||
            strcmp(current->name, "sinteger") == 0 ||
            strcmp(current->name, "uinteger") == 0 ||
            strcmp(current->name, "int") == 0 ||
            strcmp(current->name, "int32") == 0 ||
            strcmp(current->name, "int64") == 0) {
            is_integer_type = true;
            break;
        }
        if (!current->parent) break;
        current = type_system_lookup(ts, current->parent);
    }

    if (is_integer_type) {
        // Создаем enum
        EnumType* enum_type = type_system_create_enumtype(ts, name.c_str(), base_type->base_type, is_bitfield);
        type_spec_destroy(base_type);
        if (!enum_type) throw std::runtime_error("Failed to create enum");

        // TODO: Сохранить entries в enum_type

        return enum_type;
    }
    else {
        type_spec_destroy(base_type);
        throw std::runtime_error("Enum base type must be integer type");
    }
}