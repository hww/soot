#include "Register.hpp"
#include "common/soot/ListBuilder.hpp"
#include "TypeSystem.hpp"

namespace soot {

Object Register::get_at(const Object &key) {

    if (type_name.is_none()) {
        throw std::runtime_error("Register expects non empt type field");
        return Object::make_none();
    }

    // --- 1. Обработка СТРОКОВЫХ ключей (Поля) ---
    if (key.type == ObjectType::SYMBOL || key.type == ObjectType::STRING) {
        std::string name = key.to_std_string();
        // printf("DEBUG: Accessing field [%s] on Alias(type=%s, offset=%d, bit=%d)\n",
        // name.c_str(),
        //        type_name.c_str(), this->offset, this->bit_offset);
        //  Мета-свойства алиаса
        if (name == ":reg")
            return reg;
        if (name == ":offset")
            return Object::make_integer(this->offset);
        if (name == ":bit_offset")
            return Object::make_integer(this->bit_offset);
        if (name == ":bit_size")
            return Object::make_integer(this->bit_size);
        if (name == ":type_name")
            return this->type_name;

        if (name == ":type")
            return TypeSystem::instance().get_at(type_name);

        Type *type_ptr = TypeSystem::instance().lookup_type(type_name.to_std_string());

        if (auto struct_ptr = dynamic_cast<StructureType *>(type_ptr)) {
            Field field;
            if (struct_ptr->lookup_field(name, &field)) {
                auto field_type_name = field.type().base_type();
                auto next_step = std::make_shared<Register>();
                next_step->reg = this->reg;
                next_step->type_name =
                    Object::make_symbol(field_type_name); // Переходим к типу поля
                next_step->offset = this->offset + field.offset();
                // получить размер поля
                auto bt = TypeSystem::instance().lookup_type(field_type_name);
                next_step->bit_size = bt->get_size_in_memory() * 8;
                return Object::make_heap_obj(next_step);
            }
        }

        if (auto bit_ptr = dynamic_cast<BitFieldType *>(type_ptr)) {
            BitField bf;
            if (bit_ptr->lookup_field(name, &bf)) {
                auto next_step = std::make_shared<Register>();
                next_step->reg = this->reg;
                next_step->type_name = Object::make_symbol(bf.type().base_type());

                // bf.offset() — это абсолютный бит от начала структуры
                // Итоговое байтовое смещение:
                next_step->offset = this->offset + (bf.offset() / 8);

                // Остаток от деления — это сдвиг внутри байта/слова:
                next_step->bit_offset = bf.offset() % 8;
                next_step->bit_size = bf.size();

                return Object::make_heap_obj(next_step);
            }
        }

        if (auto enum_ptr = dynamic_cast<EnumType *>(type_ptr)) {
            const auto &entries = enum_ptr->entries();
            auto        it = entries.find(name);

            if (it != entries.end()) {
                auto next_step = std::make_shared<Register>();
                next_step->reg = this->reg;

                // Для bitfield-энума значение — это позиция бита
                if (enum_ptr->is_bitfield()) {
                    int total_bit_offset = static_cast<int>(it->second);

                    // Вычисляем байтовое смещение и остаток в битах
                    next_step->offset = this->offset + (total_bit_offset / 8);
                    next_step->bit_offset = total_bit_offset % 8;
                    next_step->bit_size = 1;
                    next_step->type_name = Object::make_symbol("bool"); // Флаг всегда булев
                } else {
                    // Если это не битфилд, это просто константа,
                    // но мы всё равно можем вернуть алиас на это значение
                    next_step->offset = this->offset;
                    next_step->type_name = Object::make_symbol(enum_ptr->get_name());
                }
                return Object::make_heap_obj(next_step);
            }
        }
    }

    // --- 2. Обработка ЧИСЛОВЫХ ключей (Индексация) ---
    if (key.type == ObjectType::INT) {
        int64_t index = key.integer_obj.value;

        // Получаем информацию о текущем типе
        auto type_ptr = TypeSystem::instance().lookup_type(type_name.to_std_string());
        if (!type_ptr)
            return Object::make_none();

        int stride = type_ptr->get_size_in_memory();

        // Учитываем специфическое выравнивание структур в памяти
        if (auto struct_ptr = dynamic_cast<StructureType *>(type_ptr)) {
            stride = struct_ptr->get_inline_array_stride_alignment();
        }

        auto next_step = std::make_shared<Register>();
        next_step->reg = this->reg;
        next_step->type_name = this->type_name; // Тип элемента остается прежним

        // Накопление смещения: текущий offset + (индекс * размер типа)
        next_step->offset = this->offset + (static_cast<int>(index) * stride);

        // Копируем метаданные битфилдов, если они были (на случай массива битфилдов)
        next_step->bit_offset = this->bit_offset;
        next_step->bit_size = this->bit_size;

        return Object::make_heap_obj(next_step);
    }
    throw std::runtime_error(fmt::format("Register for type `{}` expects a valid key, got `{}`",
                                         type_name.to_std_string(), key.print()));
    return Object::make_none();
}

Object Register::inspect() const {
    ListBuilder lb;
    lb.add_symbol("reg-alias");
    lb.add_key_value("name", name);
    lb.add_key_value("physical-reg", reg);
    lb.add_key_value("type-name", type_name);
    lb.add_key_value("offset", Object::make_integer(offset));
    lb.add_key_value("bit-offset", Object::make_integer(bit_offset));
    lb.add_key_value("bit-ize", Object::make_integer(bit_size));
    return lb.build();
}

std::string Register::print() const {
    return fmt::format("#<reg-alias {} :type {} :reg {} :offset {} :bit-offset {} :bit-size {}>",
                       name.print(), type_name.print(), reg.print(), offset, bit_offset, bit_size);
}
} // namespace soot
