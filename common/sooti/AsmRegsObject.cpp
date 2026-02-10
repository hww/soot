#include "AsmRegsObject.hpp"
#include "ListBuilder.hpp"
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

Object AsmRegsObject::make_step_accessor(const Object &key) {
    std::string name = key.to_std_string();

    // 1. Базовые свойства
    if (name == ".alias_count")
        return Object::make_integer(this->aliases.size());

    return get_at(key);
}

Object RegisterAlias::inspect() const {
    ListBuilder lb;
    lb.add_symbol("reg-alias");
    lb.add_key_value("name", name);
    lb.add_key_value("source", source);
    lb.add_key_value("physical-reg", reg);
    lb.add_key_value("type-name", Object::make_string(type_name));
    lb.add_key_value("offset", Object::make_integer(offset));
    lb.add_key_value("bit-offset", Object::make_integer(bit_offset));
    lb.add_key_value("bit-ize", Object::make_integer(bit_size));
    return lb.build();
}
} // namespace script
