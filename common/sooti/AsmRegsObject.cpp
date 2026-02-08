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

    // 1. Базовые свойства
    if (name == ".reg" || name == ".physical_reg")
        return physical_reg;
    if (name == ".offset")
        return Object::make_integer(offset);
    if (name == ".type_name")
        return Object::make_string(type_name);

    if (name == ".type") {
        auto type_ptr = TypeSystem::instance().lookup_type(type_name);

        if (type_ptr) {
            // Если твои типы хранятся как shared_ptr в TypeSystem, просто отдавай его.
            // Если как unique_ptr, то возвращай NativeRef с пустым делетером (но помни о рисках!)
            return Object::make_native_ref(std::shared_ptr<Type>(type_ptr, [](Type *) {}));
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