#include "defenum.h"
#include "common/util/bit_utils.h"
#include "common/util/string_util.h"
#include "fmt/format.h"

namespace {

    std::string symbol_string(const script::Object& obj) {
        if (obj.is_symbol()) {
            return obj.as_symbol().name_ptr;
        }
        throw std::runtime_error(obj.print() + " was supposed to be a symbol, but isn't");
    }

    bool is_type(const std::string& expected, const TypeSpec& actual, TypeSystem* ts) {
        return ts->tc(ts->make_typespec(expected), actual);
    }

    TypeSpec parse_typespec(TypeSystem* ts, const script::Object& src) {
        if (src.is_symbol()) {
            return ts->make_typespec(symbol_string(src));
        }
        else if (src.is_pair()) {
            TypeSpec tspec = ts->make_typespec(symbol_string(src.as_pair()->car));
            const auto* rest = &src.as_pair()->cdr;

            while (rest->is_pair()) {
                auto& it = rest->as_pair()->car;
                TypeSpec arg_spec = parse_typespec(ts, it);
                tspec.add_arg(arg_spec);
                rest = &rest->as_pair()->cdr;
            }
            return tspec;
        }
        else {
            throw std::runtime_error("invalid typespec: " + src.print());
        }
    }

    bool local_integer_fits(int64_t value, int size, bool is_signed) {
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

} // namespace

// The form is 
// (defenum simple (entry1) (entry2) (entry3))
// but this method invoked with
// (simple (entry1) (entry2) (entry3))
EnumType* parse_defenum(const script::Object& defenum,
    TypeSystem* ts,
    DefinitionMetadata* symbol_metadata) {
    // Базовая валидация - defenum должен быть списком
    if (!defenum.is_pair()) {
        throw std::runtime_error("defenum must be list, got: " + defenum.print());
    }

    TypeSpec base_type = ts->make_typespec("int64");
    bool is_bitfield = false;
    std::unordered_map<std::string, int64_t> entries;

    const script::Object* iter = &defenum;

    // Проверяем первый элемент - должен быть именем
    auto& name_obj = iter->as_pair()->car;
    iter = &iter->as_pair()->cdr;
    if (!name_obj.is_symbol()) {
        throw std::runtime_error("defenum name must be a symbol");
    }

    std::string name = symbol_string(name_obj);

    // Проверяем docstring
    std::optional<std::string> maybe_docstring;
    if (iter->is_pair() && iter->as_pair()->car.is_string()) {
        if (symbol_metadata) {
            maybe_docstring = iter->as_pair()->car.as_string();
            symbol_metadata->docstring = *maybe_docstring;
        }
        iter = &iter->as_pair()->cdr;
    }

    // Парсим опции (начинаются с :)
    while (!iter->is_empty_list() && iter->is_pair()) {
        auto& current = iter->as_pair()->car;

        if (!current.is_symbol() || !current.as_symbol().starts_with_colon()) {
            break; // не опция, переходим к entries
        }

        auto option_name = symbol_string(current);
        iter = &iter->as_pair()->cdr;

        if (iter->is_empty_list() || !iter->is_pair()) {
            throw std::runtime_error("Option " + option_name + " needs value");
        }

        auto& option_value = iter->as_pair()->car;
        iter = &iter->as_pair()->cdr;

        if (option_name == ":type") {
            base_type = parse_typespec(ts, option_value);
        }
        else if (option_name == ":bitfield") {
            if (option_value.is_symbol() && option_value.is_boolean()) {
                is_bitfield = option_value.is_true();
            }
            else {
                throw std::runtime_error(":bitfield value must be #t or #f, got: " + option_value.print());
            }
        }
        else if (option_name == ":copy-entries") {
            auto other_info = ts->try_enum_lookup(parse_typespec(ts, option_value));
            if (!other_info) {
                throw std::runtime_error("Cannot copy entries from " + option_value.print() + ", it is not a valid enum type");
            }
            for (auto& e : other_info->entries()) {
                if (entries.find(e.first) != entries.end()) {
                    throw std::runtime_error("Entry " + e.first + " appears multiple times");
                }
                entries[e.first] = e.second;
            }
        }
        else {
            throw std::runtime_error("Unknown option " + option_name + " for defenum");
        }
    }

    // Парсим entries
    auto type = ts->lookup_type(base_type);
    int64_t highest = -1;

    while (!iter->is_empty_list()) {
        if (!iter->is_pair()) {
            throw std::runtime_error("invalid list structure in defenum entries");
        }

        auto& field = iter->as_pair()->car;

        if (field.is_symbol()) {
            // Простой символ: name
            //auto entry_name = symbol_string(field);
            //
            //if (entries.find(entry_name) != entries.end()) {
            //    throw std::runtime_error("Entry " + entry_name + " appears multiple times");
            //}
            //
            //entries[entry_name] = ++highest;
            throw std::runtime_error("Enum entry name must be pair, got: " + field.print());
        }
        else if (field.is_pair()) {
            // Пара: (name value)
            auto& field_pair = *field.as_pair();

            if (!field_pair.car.is_symbol()) {
                throw std::runtime_error("Enum entry name must be symbol, got: " + field_pair.car.print());
            }

            auto entry_name = symbol_string(field_pair.car);

            if (entries.find(entry_name) != entries.end()) {
                throw std::runtime_error("Entry " + entry_name + " appears multiple times");
            }

            // Получаем значение
            if (field_pair.cdr.is_empty_list()) {
                // Авто-инкремент: (name)
                entries[entry_name] = ++highest;
            }
            else if (field_pair.cdr.is_pair()) {
                // Явное значение: (name value)
                auto& value_obj = field_pair.cdr.as_pair()->car;

                if (!value_obj.is_integer()) {
                    throw std::runtime_error("Expected integer for enum value, got: " + value_obj.print());
                }

                auto entry_val = value_obj.as_integer();
                if (!integer_fits(static_cast<int64_t>(entry_val), type->get_load_size(), type->get_load_signed())) {
                    fmt::print("Warning: Integer {} does not fit inside a {}\n", entry_val, type->get_name());
                }

                if (entries.empty()) {
                    highest = entry_val;
                }
                highest = std::max(highest, entry_val);
                entries[entry_name] = entry_val;
            }
            else {
                throw std::runtime_error("Invalid enum entry format: " + field.print());
            }

        }
        else {
            throw std::runtime_error("Enum entry must be symbol or list, got: " + field.print());
        }

        iter = &iter->as_pair()->cdr;
    }

    // Создаем enum type
    if (is_type("integer", base_type, ts)) {
        auto parent_type = ts->lookup_type(base_type.base_type());
        if (auto parent_value = dynamic_cast<ValueType*>(parent_type)) {
            auto new_type = std::make_unique<EnumType>(parent_value, name, is_bitfield, entries);
            if (maybe_docstring) {
                new_type->m_metadata.docstring = *maybe_docstring;
            }
            new_type->set_runtime_name(parent_value->get_runtime_name());
            return dynamic_cast<EnumType*>(ts->add_type(name, std::move(new_type)));
        }
        else {
            throw std::runtime_error("Parent type " + base_type.print() + " is not a ValueType");
        }
    }
}