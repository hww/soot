#pragma once

#include "common/type_system/Type.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include "sootc/IR/StaticObject.hpp"
#include <string>
#include <optional>

using namespace carbon::files;

namespace sootc {

class IR_Reg;
class IR_LoadField;
class IR_LoadConst;
class IR_LoadConst;
class IR_LoadField;
class TypeEnv;
class MethodEnv;
class StateEnv;
class Compiler;

class IR_Value {
  public:
    explicit IR_Value(Type *type) : type_(type) {}
    virtual ~IR_Value() = default;
    virtual std::string to_string() const = 0;
    virtual bool        is_reg() const {
        return false;
    }
    virtual bool is_const() const {
        return false;
    }
    virtual bool is_field() const {
        return false;
    }
    Type *get_type() const {
        return type_;
    }
    virtual IR_Reg* to_reg(class Env& env) = 0;

    // Фаза 2: Наполнение логикой (IR-узлами)
    virtual void resolve(Compiler* c) { (void)c; }

    // Фаза 3: Генерация байткода
    // Возвращает пару {тип_определения, буфер}
    virtual RelocatableBuffer build(Compiler* c);

    
  protected:
    Type *type_;
};

class IR_Reg : public IR_Value {
  public:
    IR_Reg(Type *type, u32 index, bool is_arg = false);
    std::string to_string() const override;
    bool        is_reg() const override {
        return true;
    }
    u32 get_index() const {
        return index_;
    }
    bool is_argument() const {
        return is_arg_;
    }
    static constexpr u32 REG_SELF = 24;
    static constexpr u32 REG_RETURN = 0;

    IR_Reg* to_reg(Env&)  override { 
        return this; 
    }    
  private:
    u32  index_;
    bool is_arg_;
};

class IR_FunctionValue : public IR_Value {
public:
    explicit IR_FunctionValue(class FunctionEnv* env) 
        : IR_Value(nullptr), m_env(env) {}

    class FunctionEnv* get_env() const { return m_env; }
    
    // Виртуальные методы из базового класса
    class IR_Reg* to_reg(class Env&) override { return nullptr; }
    std::string to_string() const override { return "function_ptr"; }

    void resolve(Compiler* c) override; 
    RelocatableBuffer build(Compiler* c) override;

private:
    class FunctionEnv* m_env;
};

class IR_Const : public IR_Value {
  public:
    IR_Const(Type *type, s64 val);
    IR_Const(Type *type, float val);
    std::string to_string() const override;
    bool        is_const() const override {
        return true;
    }
    bool is_float() const {
        return is_float_;
    }
    s64 get_int() const {
        return int_val_;
    }
    float get_float() const {
        return float_val_;
    }
    IR_Reg* to_reg(Env& env) override;
  private:
    s64   int_val_ = 0;
    float float_val_ = 0.0f;
    bool  is_float_ = false;
};

class IR_Field : public IR_Value {
  public:
    IR_Field(IR_Value *base, const Field &field);
    std::string to_string() const override;
    bool        is_field() const override {
        return true;
    }
    IR_Value *get_base() const {
        return base_;
    }
    const Field &get_field() const {
        return field_;
    }
    int get_offset() const {
        return field_.offset();
    }
    IR_Reg* to_reg(Env& env) override;
  private:
    IR_Value *base_;
    Field     field_;
};

class IR_MethodValue : public IR_Value {
public:
    // Теперь хранит ссылку на MethodEnv (который наследует FunctionEnv)
    explicit IR_MethodValue(MethodEnv* env) 
        : IR_Value(nullptr), m_env(env) {}
    
    MethodEnv* get_env() const { return m_env; }
    
    std::string name() const;
    std::string type_name() const;
    
    std::string to_string() const override { 
        return "method:" + type_name() + "." + name(); 
    }
    
    IR_Reg* to_reg(Env& env) override {
        // Метод как значение — в VM это указатель на функцию
        // Можно вернуть регистр с адресом метода
        (void)env;
        return nullptr;  // TODO: реализовать
    }
    
private:
    MethodEnv* m_env;  // ← ссылка на окружение метода
};


class IR_StateValue : public IR_Value {
public:
    // Теперь хранит ссылку на StateEnv (который наследует FunctionEnv)
    explicit IR_StateValue(StateEnv* env) 
        : IR_Value(nullptr), m_env(env) {}
    
    StateEnv* get_env() const { return m_env; }
    
    std::string name() const;
    std::string type_name() const;
    
    std::string to_string() const override { 
        return "method:" + type_name() + "." + name(); 
    }
    
    IR_Reg* to_reg(Env& env) override {
        // Метод как значение — в VM это указатель на функцию
        // Можно вернуть регистр с адресом метода
        (void)env;
        return nullptr;  // TODO: реализовать
    }
    
private:
    StateEnv* m_env;  // ← ссылка на окружение метода
};

class IR_Type : public IR_Value {
public:
    // Конструктор: принимаем указатель на метаданные типа из TypeSystem
    explicit IR_Type(TypeEnv* represented_type) 
        : IR_Value(nullptr), m_env(represented_type) {}

    // Позволяет компилятору достать StructureType и посмотреть поля/методы
    TypeEnv* get_represented_type() const { return m_env; }
    
    std::string to_string() const override;

    // Тип сам по себе не является значением, которое можно положить в регистр 
    // (если только ты не делаешь рефлексию в рантайме), поэтому возвращаем nullptr
    IR_Reg* to_reg(class Env&) override { 
        return nullptr; 
    }

    TypeEnv* get_env() const {
        return m_env;
    }

    void resolve(Compiler* c) override;
    RelocatableBuffer build(Compiler* c) override;

private:
    TypeEnv* m_env;
};

class IR_None : public IR_Value {
public:
    IR_None() : IR_Value(nullptr) {}
    std::string to_string() const override { return "none"; }
    IR_Reg* to_reg(Env& env) override { (void)env; return nullptr; }
};



// Для define-extern 
class IR_ExternValue : public IR_Value {
public:
    IR_ExternValue(const std::string& name, const TypeSpec& type_spec)
        : IR_Value(nullptr), m_name(name), m_type_spec(type_spec) {}
    
    std::string name() const { return m_name; }
    TypeSpec type_spec() const { return m_type_spec; }
    
    std::string to_string() const override {
        return "extern:" + m_name;
    }
    
    IR_Reg* to_reg(Env& env) override {
        (void)env;
        // Внешние символы не могут быть использованы напрямую в рантайме?
        // Возвращаем nullptr или загружаем из глобальной таблицы
        return nullptr;
    }
    
    RelocatableBuffer build(Compiler* c) override {
        (void)c;
        // Внешние символы не генерируют код
        return RelocatableBuffer();
    }
    
private:
    std::string m_name;
    TypeSpec m_type_spec;
};


class IR_StaticValue : public IR_Value {
public:
    IR_StaticValue(StaticObject* obj, Type* type) 
        : IR_Value(type), m_obj(obj) {}
    
    std::string to_string() const override {
        return m_obj->print();
    }
    
    IR_Reg* to_reg(Env& env) override {
        (void)env;
        // Загрузка статического объекта в регистр
        return nullptr; // Реализовать позже
    }
    
    RelocatableBuffer build(Compiler* c) override;
    
    StaticObject* get_object() const { return m_obj; }
    
private:
    StaticObject* m_obj;
};


} // namespace sootc
