#pragma once

#include "common/sootc/Env/FunctionEnv.hpp"
#include "common/sootc/Env/Env.hpp"

namespace sootc {

class TypeEnv;

class MethodEnv : public FunctionEnv {
public:
    MethodEnv(const std::string& name, Env* parent, Type* type, TypeEnv* type_env = nullptr)
        : FunctionEnv(parent, name), m_type(type), m_type_env(type_env) {
        // this — первый аргумент (регистр 0)
        define_argument("this", type, 0);
        
        // Устанавливаем тип окружения
        m_kind = EnvKind::METHOD_ENV;
    }

    const Type* type() const { return m_type; }
    TypeEnv* type_env() const { return m_type_env; }
    void set_type_env(TypeEnv* env) { m_type_env = env; }
    
    // Для отладки
    std::string print() const override {
        return fmt::format("MethodEnv(name={}, type={})", 
                           name(), m_type ? m_type->get_name() : "unknown");
    }
    
private:    
    Type* m_type;
    TypeEnv* m_type_env = nullptr;  // обратная ссылка на тип
};

} // namespace sootc