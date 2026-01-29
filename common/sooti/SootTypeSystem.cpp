#include "common/sooti/SootTypeSystem.hpp"
// Сначала полные определения зависимостей:
#include "common/sooti/Interpreter.hpp" 
#include "common/sooti/Object.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/type_system/Deftype.hpp"
#include "common/type_system/Defenum.hpp"
#include "common/type_system/TypeSpec.hpp"

namespace script {

// Используем make_shared, так как поле m_type_system - это shared_ptr
SootTypeSystem::SootTypeSystem(Interpreter& interpreter) 
    : m_type_system(std::make_shared<TypeSystem>()), m_interpreter(interpreter) {}

SootTypeSystem::~SootTypeSystem() = default;

void SootTypeSystem::init_type_system(BaseTyles types) {
    m_type = types;
    m_type_system->clear();
    
    switch (types) {
        case BaseTyles::Z80:
            m_type_system->add_builtin_types_z80();
            break;
        case BaseTyles::Default:
        default:
            m_type_system->add_builtin_types();
            break;
    }

    m_interpreter.define_var_in_env(m_interpreter.get_global_environment(), 
                    m_type_system.get()->to_alias(), 
                    "*type-system*");
}

Object SootTypeSystem::eval_deftype_special(const Object&, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    auto env_ptr = m_interpreter.get_global_environment().as_env();
    parse_deftype(rest, m_type_system.get(), &env_ptr->vars);
    return m_interpreter.get_null();
}

Object SootTypeSystem::eval_defenum_special(const Object&, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    parse_defenum(rest, m_type_system.get(), nullptr);
    return m_interpreter.get_null();
}

Object SootTypeSystem::eval_typespec_special(const Object&, const Object& rest, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    if (rest.is_empty_list()) return m_interpreter.get_null();

    Object spec_input = rest.as_pair()->car;
    auto ts = std::make_shared<TypeSpec>(parse_typespec(m_type_system.get(), spec_input));
    
    return Object::make_native_ref(ts);
}

// Исправлено соответствие сигнатуры (Arguments& args)
Object SootTypeSystem::eval_types_list(const Object&, Arguments&, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    return m_type_system->get_all_type_names_as_objects();
}

Object SootTypeSystem::eval_init_types(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env) {
    (void)env;
    // Здесь должна быть логика из твоего старого кода для init-types
    if (args.unnamed.size() > 0 && args.unnamed[0].as_symbol() == "z80") {
        init_type_system(BaseTyles::Z80);
    } else {
        init_type_system(BaseTyles::Default);
    }
    return m_interpreter.get_null();
}

} // namespace script
