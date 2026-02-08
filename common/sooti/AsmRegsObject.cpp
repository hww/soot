#include "AsmRegsObject.hpp"
#include "common/type_system/TypeSystem.hpp"

namespace script {

Object get_current_asm_context(std::shared_ptr<EnvironmentObject> env) {
    auto current = env;
    while (current) {
        if (auto asm_env = std::dynamic_pointer_cast<AsmEnvironmentObject>(current)) {
            return asm_env->get_asm_context();
        }
        current = current->parent_env;
    }
    return Object::make_none();
}
Object RegisterAlias::make_step_accessor(const Object &key) {
    auto type_ptr = TypeSystem::instance().lookup_type(type_name);
    if (!type_ptr)
        return Object::make_none();

    // --- 1. Обработка СТРОКОВЫХ ключей (Поля) ---
    if (key.type == ObjectType::SYMBOL || key.type == ObjectType::STRING) {
        std::string name = key.to_std_string();
        printf("DEBUG: Accessing field [%s] on Alias(type=%s, offset=%d, bit=%d)\n", name.c_str(),
               type_name.c_str(), this->offset, this->bit_offset);
        // Мета-свойства алиаса
        if (name == ".reg" || name == ".physical_reg")
            return physical_reg;
        if (name == ".offset")
            return Object::make_integer(this->offset);
        if (name == ".bit_offset")
            return Object::make_integer(this->bit_offset);
        if (name == ".bit_size")
            return Object::make_integer(this->bit_size);
        if (name == ".type")
            return Object::make_string(this->type_name);

        if (auto struct_ptr = dynamic_cast<StructureType *>(type_ptr)) {
            Field field;
            if (struct_ptr->lookup_field(name, &field)) {
                auto next_step = std::make_shared<RegisterAlias>();
                next_step->physical_reg = this->physical_reg;
                next_step->type_name = field.type().base_type(); // Переходим к типу поля
                next_step->offset = this->offset + field.offset();
                return Object::make_native_ref(next_step);
            }
        }

        if (auto bit_ptr = dynamic_cast<BitFieldType *>(type_ptr)) {
            BitField bf;
            if (bit_ptr->lookup_field(name, &bf)) {
                auto next_step = std::make_shared<RegisterAlias>();
                next_step->physical_reg = this->physical_reg;
                next_step->type_name = bf.type().base_type();

                // bf.offset() — это абсолютный бит от начала структуры
                // Итоговое байтовое смещение:
                next_step->offset = this->offset + (bf.offset() / 8);

                // Остаток от деления — это сдвиг внутри байта/слова:
                next_step->bit_offset = bf.offset() % 8;
                next_step->bit_size = bf.size();

                return Object::make_native_ref(next_step);
            }
        }

        if (auto enum_ptr = dynamic_cast<EnumType *>(type_ptr)) {
            const auto &entries = enum_ptr->entries();
            auto        it = entries.find(name);

            if (it != entries.end()) {
                auto next_step = std::make_shared<RegisterAlias>();
                next_step->physical_reg = this->physical_reg;

                // Для bitfield-энума значение — это позиция бита
                if (enum_ptr->is_bitfield()) {
                    int total_bit_offset = static_cast<int>(it->second);

                    // Вычисляем байтовое смещение и остаток в битах
                    next_step->offset = this->offset + (total_bit_offset / 8);
                    next_step->bit_offset = total_bit_offset % 8;
                    next_step->bit_size = 1;
                    next_step->type_name = "bool"; // Флаг всегда булев
                } else {
                    // Если это не битфилд, это просто константа,
                    // но мы всё равно можем вернуть алиас на это значение
                    next_step->offset = this->offset;
                    next_step->type_name = enum_ptr->get_name();
                }
                return Object::make_native_ref(next_step);
            }
        }
    }

    // --- 2. Обработка ЧИСЛОВЫХ ключей (Индексация) ---
    if (key.type == ObjectType::INTEGER) {
        int64_t     index = key.integer_obj.value;
        int         stride = 0;
        std::string element_type = type_name;

        // ВАРИАНТ А: Тип сам по себе является массивом (напр. inline-array)
        // В этом случае мы должны "распаковать" его и узнать тип элемента
        /* if (auto array_ptr = dynamic_cast<ArrayType*>(type_ptr)) {
            stride = array_ptr->get_stride();
            element_type = array_ptr->get_element_type_name();
        }
        */

        // ВАРИАНТ Б: Мы индексируем указатель или структуру как массив (Pointer Arithmetic)
        // Если это не специальный ArrayType, берем размер самого типа как шаг
        if (stride == 0) {
            stride = type_ptr->get_size_in_memory();

            // Если тип - StructureType, учитываем его специфическое выравнивание в массиве
            if (auto struct_ptr = dynamic_cast<StructureType *>(type_ptr)) {
                stride = struct_ptr->get_inline_array_stride_alignment();
                // Обычно stride для структур кратен 16 байтам
            }
        }

        auto next_step = std::make_shared<RegisterAlias>();
        next_step->physical_reg = this->physical_reg;
        next_step->type_name =
            element_type; // Элемент массива имеет тот же тип (или тип из ArrayType)
        next_step->offset = this->offset + (static_cast<int>(index) * stride);

        return Object::make_native_ref(next_step);
    }

    return Object::make_none();
}

Object AsmRegsObject::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Базовые свойства
    if (name == ".alias_count")
        return Object::make_integer(this->aliases.size());

    return get_at(key);
}
} // namespace script