#pragma once

#include "common/sootc/Env/FunctionEnv.hpp"
#include "common/sootc/Env/Env.hpp"

namespace sootc {

class TypeEnv;

class MethodEnv : public FunctionEnv {
public:
    MethodEnv(u32 id, const std::string& name, Env* parent, Type* type)
        : FunctionEnv(parent, name), m_id(id), m_type(type), 
        m_type_env(parent ? parent->type_env() : nullptr)
    {
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
                           get_name(), m_type ? m_type->name() : "unknown");
    }
    u32 id() const { return m_id; }
    void set_defined(bool defined) { m_is_defined = defined; }
    bool is_defined() const { return m_is_defined; }
private:    
    u32 m_id = 0;  
    Type* m_type;
    TypeEnv* m_type_env = nullptr;  // обратная ссылка на тип
    bool m_is_defined = false;
};

} // namespace sootc