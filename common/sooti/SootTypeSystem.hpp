#pragma once

#include "common/sooti/Reader.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/Interpreter.hpp"

class TypeSystem;
class Type;
class TypeSpec;

namespace script {

class SootTypeSystem
{
    friend class Interpreter;

public:
    SootTypeSystem() = default;
    SootTypeSystem(Interpreter& interpreter);
    ~SootTypeSystem();
    void init_type_system();

    Object eval_defenum_special(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_deftype_special(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);
    Object eval_typespec_special(const Object& form, const Object& rest, const std::shared_ptr<EnvironmentObject>& env);

    Object eval_type_to_lisp(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

    Object eval_types_list(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env);

    private:
    Object type_spec_to_lisp(const TypeSpec& ts) const;
    Object type_to_lisp(const Type* type) const;
    Object build_list(const std::vector<Object>& objects) const;
    
    std::unique_ptr<TypeSystem> m_type_system;
    Interpreter& m_interpreter;
};
}