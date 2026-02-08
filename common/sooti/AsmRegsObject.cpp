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
    std::string name = key.to_std_string();

    // 1. Внутренняя рефлексия алиаса
    if (name == ".reg" || name == ".physical_reg")
        return physical_reg;
    if (name == ".offset")
        return Object::make_integer(offset);
    if (name == ".type_name")
        return Object::make_string(type_name);

    // 2. Получаем объект типа из глобальной таблицы
    auto type_ptr = TypeSystem::instance().lookup_type(type_name);
    if (!type_ptr)
        return Object::make_none();

    // 3. Пытаемся найти поле (сначала в структурах)
    auto struct_ptr = dynamic_cast<StructureType *>(type_ptr);
    if (struct_ptr) {
        Field field;
        if (struct_ptr->lookup_field(name, &field)) {
            // Создаем новый алиас - "шаг вглубь"
            auto next_step = std::make_shared<RegisterAlias>();

            // Наследуем физический регистр
            next_step->physical_reg = this->physical_reg;

            // Устанавливаем тип этого поля для следующего шага
            next_step->type_name = field.type().base_type();

            // ГЛАВНОЕ: Аккумулируем смещение
            next_step->offset = this->offset + field.offset();

            return Object::make_native_ref(next_step);
        }
    }

    // 4. Поддержка битфилдов (если нужно)
    auto bit_ptr = dynamic_cast<BitFieldType *>(type_ptr);
    if (bit_ptr) {
        BitField bf;
        if (bit_ptr->lookup_field(name, &bf)) {
            auto next_step = std::make_shared<RegisterAlias>();
            next_step->physical_reg = this->physical_reg;
            next_step->type_name = bf.type().base_type();
            // Смещение битфилда (приводим биты к байтам, если это допустимо в вашей архитектуре)
            next_step->offset = this->offset + (bf.offset() / 8);
            return Object::make_native_ref(next_step);
        }
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