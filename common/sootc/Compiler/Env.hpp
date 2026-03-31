#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "common/sootc/IR/IR_Value.hpp"
#include "common/type_system/Type.hpp"
#include "common/type_system/TypeSpec.hpp"
#include "common/carbon/files/BinaryFileBuilder.hpp"

using namespace carbon::files;

namespace sootc {

class IR_Node;

enum class EnvKind {
    GLOBAL,
    FUNCTION,
    METHOD,
    LEXICAL
};

class FunctionEnv;
class GlobalEnv;
class IR_Method;

class Env {
public:
    explicit Env(EnvKind kind, Env* parent = nullptr);
    virtual ~Env() = default;

    virtual IR_Value* lookup(const std::string& name);
    void bind(const std::string& name, IR_Value* val);
    virtual void emit(IR_Node* node);

    // Геттеры для компилятора
    Env* parent() const { return m_parent; }
    EnvKind kind() const { return m_kind; }
    const std::unordered_map<std::string, IR_Value*>& symbols() const { return m_symbols; }
    FunctionEnv* function_env();
    GlobalEnv* global_env();

protected:
    EnvKind m_kind;
    Env* m_parent;
    std::unordered_map<std::string, IR_Value*> m_symbols;
};

class GlobalEnv : public Env {
public:
    GlobalEnv();
    void add_type(const std::string& name, Type* type);
    Type* lookup_type(const std::string& name);
private:
    std::unordered_map<std::string, Type*> m_types;
};

class LexicalEnv : public Env {
public:
    explicit LexicalEnv(Env* parent);
};


class FunctionEnv : public Env {
public:
    FunctionEnv(Env* parent, const std::string& name);

    void define_argument(const std::string& name, Type* type, int reg_index);
    void emit(IR_Node* node) override;

    // Выделение временных регистров (r0, r1...)
    IR_Reg* alloc_reg(Type* type) {
        return new IR_Reg(type, m_next_reg++);
    }

    const std::vector<IR_Value*>& params() const { return m_params; }
    const std::vector<IR_Node*>& nodes() const { return m_nodes; }
    const std::string& name() const { return m_name; }

    void set_source_form(const script::Object& forms) {
        m_source_form = forms;
    }

    script::Object source_form() const {
        return m_source_form;
    }

private:
    std::string m_name;
    int m_next_reg = 0; 
    std::vector<IR_Value*> m_params;
    std::vector<IR_Node*> m_nodes;
    script::Object m_source_form; 
};


class MethodEnv : public FunctionEnv {
public:
    MethodEnv(Env* parent, const std::string& name, Type* type)
        : FunctionEnv(parent, name), m_type(type) {
        // this — первый аргумент
        define_argument("this", type, 0);
    }
    
    const Type* type() const { return m_type; }
    
private:    
    Type* m_type;
};


class StateEnv : public FunctionEnv {
public:
    StateEnv(Env* parent, const std::string& name, Type* type)
        : FunctionEnv(parent, name), m_type(type) {
        // this — первый аргумент
        define_argument("this", type, 0);
    }
    
    const Type* type() const { return m_type; }

    // Обработчики состояния — это просто методы
    void set_enter_method(MethodEnv* method) { m_enter_method = method; }
    void set_exit_method(MethodEnv* method) { m_exit_method = method; }
    void set_code_method(MethodEnv* method) { m_code_method = method; }
    void set_post_method(MethodEnv* method) { m_post_method = method; }
    void set_event_method(MethodEnv* method) { m_event_method = method; }
    
    MethodEnv* enter_method() const { return m_enter_method; }
    MethodEnv* exit_method() const { return m_exit_method; }
    MethodEnv* code_method() const { return m_code_method; }
    MethodEnv* post_method() const { return m_post_method; }
    MethodEnv* event_method() const { return m_event_method; }

private:

    Type* m_type;
    // Ссылка на имплементацию методов (пока nullptr, будет заполнено позже)
    MethodEnv* m_enter_method = nullptr; 
    MethodEnv* m_exit_method = nullptr; 
    MethodEnv* m_code_method = nullptr; 
    MethodEnv* m_post_method = nullptr; 
    MethodEnv* m_event_method = nullptr;     
};



class TypeEnv : public Env {
public:
    TypeEnv(Type* type) : Env(EnvKind::LEXICAL), m_type(type) {
        m_methods.resize(type->methods_max_id() + 1, nullptr);
    }

    std::string get_name() const { return m_type->get_name(); }
    Type* get_type() const { return m_type; }
    
    void add_method(MethodEnv& m) { 
        MethodInfo method_info;
        if (!m_type->get_my_method(m.name(), &method_info))
            throw std::runtime_error(fmt::format("Method {} not found in type {}", m.name(), m_type->get_name()));
        m_methods[method_info.id] = &m; 
    }

    void add_state(StateEnv& s) { m_states.push_back(&s); }

    const std::vector<MethodEnv*>& methods() const { return m_methods; }
    const std::vector<StateEnv*>& states() const { return m_states; }

    // Преобразование в TypeDesc для сериализации
    TypeDesc to_type_desc() const {
        TypeDesc desc;
        desc.name = StringId(m_type->get_name());
        desc.parent_type_id = StringId(m_type->get_parent());
        desc.flags = 0;
        desc.size_in_memory = m_type->get_size_in_memory();
        desc.heap_base = m_type->heap_base();
        desc.methods_count = m_methods.size();
        desc.states_count = m_states.size();
        // methods_offset и states_offset будут заполнены при сериализации
        return desc;
    }

private:
    Type* m_type;
    std::vector<MethodEnv*> m_methods;
    std::vector<StateEnv*> m_states;
};

} // namespace sootc