// IR_Value.hpp
#pragma once

#include "common/type_system/Type.hpp"
#include "common/sootc/IR/StaticObject.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include <cassert>
#include <string>

using namespace carbon;

namespace sootc {

class IR_Reg;
class IR_LoadField;
class IR_LoadConst;
class TypeEnv;
class Env;
class MethodEnv;
class StateEnv;
class Compiler;
class IR_Value;

class IR_Value {
  public:
    explicit IR_Value(Type *type) : type_(type) {}
    virtual ~IR_Value() = default;

    Type *get_type() const { return type_; }
    std::string type_name() const { return  type_->name(); }

    virtual std::string to_string() const = 0;
    virtual bool is_reg() const { return false; }
    virtual bool is_const() const { return false; }
    virtual bool is_field() const { return false; }
    virtual bool is_buildable() const { return false; }
    virtual bool is_argument() const { return false; }

    // Фаза связывания имен
    virtual void resolve(Compiler* c) { (void)c; }

    // Главный метод: "запиши свои инструкции в контекст"  
    virtual void emit(Env&, Compiler*) { assert(false && "Cannot generate code for this value type"); }

    // Запиши результат в файл  
    virtual ProgramBinaryElement serialize(Compiler*) { return ProgramBinaryElement{0}; }

    // Список операндов для оптимизаций или анализа графа
    virtual std::vector<IR_Value *> get_used_values() const { return {}; }

  protected:
    Type *type_;
};

// ===================================================
// ПАССИВНЫЕ ЗНАЧЕНИЯ (только данные, без генерации кода)
// ===================================================

class IR_None : public IR_Value {
public:
    IR_None() : IR_Value(nullptr) {}
    std::string to_string() const override { return "none"; }
    void resolve(Compiler* c) override { (void)c; }
};

class IR_Reg : public IR_Value {
public:
    IR_Reg(Type *type, u32 index, bool is_arg = false);

    u32 get_index() const { return index_; }

    std::string to_string() const override;   
    bool is_reg() const override { return true; }
    bool is_argument() const override { return is_arg_; }

    void resolve(Compiler* c) override { (void)c; }
 
    static constexpr u32 REG_SELF = 24;
    static constexpr u32 REG_RETURN = 0;
    
private:
    u32  index_;
    bool is_arg_;
};

class IR_Const : public IR_Value {
public:
    static IR_Const* create_int(Type* type, i64 val);
    static IR_Const* create_float(Type* type, f64 val);

    i64 get_int() const { return int_val_; }
    float get_float() const { return float_val_; }
    
    std::string to_string() const override;
    bool is_const() const override { return true; }
    bool is_float() const { return is_float_; }

    void resolve(Compiler* c) override { (void)c; }

private:
    IR_Const(Type* type, i64 val) 
        : IR_Value(type), int_val_(val), is_float_(false) {}
    IR_Const(Type* type, f64 val) 
        : IR_Value(type), float_val_(val), is_float_(true) {}

    union {
        i64 int_val_;
        float float_val_;
    };
    bool is_float_;
};

// ===================================================
// ОПРЕДЕЛЕНИЯ (реализуют IR_IBuildable)
// ===================================================


class IR_MethodValue : public IR_Value {
public:
    explicit IR_MethodValue(MethodEnv* env);
    
    MethodEnv* get_env() const { return m_env; }
    std::string name() const { return m_name; }
    
    std::string to_string() const override { return "method:" + type_name() + "." + name(); }
    bool is_buildable() const override { return true; }
    
    void resolve(Compiler* c) override;
    ProgramBinaryElement serialize(Compiler* c) override;
    
private:
    std::string m_name;
    MethodEnv* m_env;
};

class IR_StateValue : public IR_Value {
public:
    explicit IR_StateValue(StateEnv* env);

    StateEnv* get_env() const { return m_env; }
    std::string name() const { return m_name; }
    
    std::string to_string() const override {  return "state:" + type_name() + "." + name();  }

    bool is_buildable() const override { return true; }
  
    void resolve(Compiler* c) override;
    ProgramBinaryElement serialize(Compiler* c) override;
    
private:
    std::string m_name;
    StateEnv* m_env;
};

class IR_Type : public IR_Value {
public:
    explicit IR_Type(TypeEnv* env);

   TypeEnv* get_env() const { return m_env; }
    
    std::string to_string() const override;
    bool is_buildable() const override { return true; }

    void resolve(Compiler* c) override;
    ProgramBinaryElement serialize(Compiler* c) override;

 
private:
    std::string m_name;
    TypeEnv* m_env;
};

class IR_ExternValue : public IR_Value {
public:
    IR_ExternValue(const std::string& name, const TypeSpec& type_spec)
        : IR_Value(nullptr), m_name(name), m_type_spec(type_spec) {}
    
    std::string name() const { return m_name; }
    TypeSpec type_spec() const { return m_type_spec; }
    
    std::string to_string() const override { return "extern:" + m_name; }
    
    void resolve(Compiler* c) override;
    
private:
    std::string m_name;
    TypeSpec m_type_spec;
};

class IR_StaticValue : public IR_Value {
public:
    IR_StaticValue(StaticObject* obj, Type* type) 
        : IR_Value(type), m_obj(obj) {}
    
    StaticObject* get_object() const { return m_obj; }

    std::string to_string() const override { return m_obj->print(); }
    
    void resolve(Compiler* c) override;   
    
private:
    StaticObject* m_obj;
};

class IR_LiteralValue : public IR_Value {
public:
    explicit IR_LiteralValue(const soot::Object& obj) 
        : IR_Value(nullptr), data_(obj) {}

    std::string to_string() const override { return "literal"; }
    
    // В методе resolve мы превратим это в IR_Const
    void resolve(Compiler* c) override {
        // Логика определения типа константы через c->type_system()
    }

private:
    soot::Object data_;
};

class IR_SymbolReference : public IR_Value {
public:
    // Мы принимаем soot::Object, который уже является символом
    IR_SymbolReference(soot::Object symbol, Env* env) 
        : IR_Value(nullptr), symbol_(symbol), env_(env) 
    {
        assert(symbol.is_symbol()); 
    }

    std::string to_string() const override { 
        // Кастим в строку только для логов
        return "ref:" + symbol_.to_std_string(); 
    }

    void resolve(Compiler* c) override;

private:
    soot::Object symbol_; 
    Env* env_;
};



class StackVarAddrVal : public IR_Value {
public:
    StackVarAddrVal(Type* type, int slot, int slot_count)
        : IR_Value(type), m_slot(slot), m_slot_count(slot_count) {}
    
    int slot() const { return m_slot; }
    int slot_count() const { return m_slot_count; }
    
    std::string to_string() const override {
        return fmt::format("stack-{}", m_slot);
    }
    
private:
    int m_slot;
    int m_slot_count;
};

} // namespace sootc