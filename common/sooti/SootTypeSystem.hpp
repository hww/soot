#pragma once

#include <memory>
// Нам нужны полные определения Object и Arguments для сигнатур методов
#include "common/sooti/Object.hpp" 

class TypeSystem;

namespace script {

class Interpreter;
class EnvironmentObject;

class SootTypeSystem {
public:
    enum class BaseTyles {
        Undefined,
        Default,
        Z80,
    };

    SootTypeSystem(Interpreter& interpreter);
    ~SootTypeSystem();

    void init_type_system(BaseTyles types);
    BaseTyles get_initialization_type() const { return m_type; }

    Object eval_defenum_special(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_deftype_special(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_typespec_special(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);

    // Убедись, что Arguments& здесь соответствует реализации
    Object eval_types_list(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_init_types(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

    TypeSystem* get_ts() { return m_type_system.get(); }
    std::shared_ptr<TypeSystem> get_shared_ts() { return m_type_system; }

private:
    std::shared_ptr<TypeSystem> m_type_system;
    Interpreter& m_interpreter;
    BaseTyles m_type = BaseTyles::Undefined;
};

} // namespace script
