#include "Compiler.hpp"
#include "common/util/Log.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Compiler/MethodCompiler.hpp"
#include "sootc/Compiler/TypeCompiler.hpp"
#include "sootc/IR/IR_Node.hpp"
#include "sootc/Compiler/Env.hpp"
#include "common/carbon/files/FunctionDesc.hpp"

using namespace carbon::files;

namespace sootc {

Compiler::Compiler(TypeSystem& ts, std::string module_name) 
    : ts_(ts), builder_(module_name) {
    setup_forms();
}

#define REGISTER_FORM(name, method) \
    m_forms[name] = [](Compiler* c, const script::Object& f, const script::Object& r, Env* e) \
                    { return c->method(f, r, e); }

void Compiler::setup_forms() {
    REGISTER_FORM("define", compile_define);
    REGISTER_FORM("lambda", compile_lambda);
    REGISTER_FORM("begin",  compile_begin);
    REGISTER_FORM("+",      compile_add);
    REGISTER_FORM("-",      compile_sub);
    REGISTER_FORM("defmethod",  compile_defmethod);
    REGISTER_FORM("deftype",  compile_deftype);
}
#undef REGISTER_FORM


// ПЕРВЫЙ ПРОХОД
std::shared_ptr<carbon::modules::Module> Compiler::compile_module(const script::Object& forms, Env* env) {
    
    // ФАЗА 1: DECLARE
    auto current = forms;
    while (current.is_pair()) {
        declare(current.as_pair()->car, env); 
        current = current.as_pair()->cdr;
    }

    // Собираем список всех значений для итерации (чтобы не было проблем с изменением map)
    std::vector<std::pair<std::string, IR_Value*>> work_list;
    for (auto& [name, value] : env->symbols()) {
        work_list.push_back({name, value});
    }

    // ФАЗА 2: RESOLVE (Полиморфно)
    for (auto& [name, value] : work_list) {
        value->resolve(this);
    }

    // ФАЗА 3: BUILD (Полиморфно)
    for (auto& [name, value] : work_list) {
        if (auto result = value->build(this)) {
            const auto& [type_tag, buffer] = *result;
            builder_.add_definition(name, type_tag, std::move(buffer));
        }
    }

    return builder_.build_module();
}


IR_Value* Compiler::declare(const script::Object& form, Env* env) {
    if (form.is_number()) return compile_number(form, env);
    if (form.is_symbol()) return compile_symbol(form, env);

    if (form.is_pair()) {
        auto pair = form.as_pair();
        if (pair->car.is_symbol()) {
            auto op = pair->car.as_symbol().c_str();
            auto it = m_forms.find(op);
            if (it != m_forms.end()) {
                return it->second(this, form, pair->cdr, env);
            }
        }
        return compile_call(form, env);
    }
    return nullptr;
}

IR_Value* Compiler::compile_add(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;
    auto args = rest;
    IR_Value* v_left = declare(args.as_pair()->car, env);
    IR_Value* v_right = declare(args.as_pair()->cdr.as_pair()->car, env);

    IR_Reg* r_left = v_left->to_reg(static_cast<Env&>(*env));
    IR_Reg* r_right = v_right->to_reg(static_cast<Env&>(*env));

    IR_Reg* dest = env->function_env()->alloc_reg(r_left->get_type());
    env->emit(new IR_Binary(IR_Binary::Op::ADD, dest, r_left, r_right));

    return dest;
}

IR_Value* Compiler::compile_sub(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;
    auto args = rest;
    IR_Value* v_left = declare(args.as_pair()->car, env);
    IR_Value* v_right = declare(args.as_pair()->cdr.as_pair()->car, env);

    IR_Reg* r_left = v_left->to_reg(static_cast<Env&>(*env));
    IR_Reg* r_right = v_right->to_reg(static_cast<Env&>(*env));

    IR_Reg* dest = env->function_env()->alloc_reg(r_left->get_type());
    env->emit(new IR_Binary(IR_Binary::Op::SUB, dest, r_left, r_right));

    return dest;
}

IR_Value* Compiler::compile_define(const script::Object&, const script::Object& rest, Env* env) {
    std::string name = rest.as_pair()->car.as_symbol().c_str();
    auto value_form = rest.as_pair()->cdr.as_pair()->car;

    // Рекурсивно компилируем (например, лямбду), получаем IR_Value
    IR_Value* val = declare(value_form, env);
    
    // Просто запоминаем: "по имени name лежит вот этот IR"
    env->bind(name, val); 
    
    return val;
}

IR_Value* Compiler::compile_begin(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;
    auto current = rest;
    IR_Value* last = nullptr;
    while (current.is_pair()) {
        last = declare(current.as_pair()->car, env);
        current = current.as_pair()->cdr;
    }
    return last;
}

IR_Value* Compiler::compile_lambda(const script::Object& form, const script::Object& rest, Env* env) {
    FunctionCompiler func_compiler(ts_, this);
    // Теперь это declare. Он создаст IR_FunctionValue и сохранит тело.
    return func_compiler.declare(form, rest, env);
}

IR_Value* Compiler::compile_defmethod(const script::Object& form, const script::Object& rest, Env* env) {
    MethodCompiler method_compiler(ts_, this);
    // Аналогично: регистрируем метод в типе.
    return method_compiler.declare(form, rest, env);
}

IR_Value* Compiler::compile_deftype(const script::Object& form, const script::Object& rest, Env* env) {
    TypeCompiler type_compiler(ts_, this);
    // Регистрируем структуру типа в TypeSystem и создаем TypeEnv.
    return type_compiler.declare(form, rest, env);
}

IR_Value* Compiler::compile_number(const script::Object& form, Env*) {
    // Используем универсальный lookup_type, так как точное имя метода в TypeSystem неизвестно
    return new IR_Const(ts_.lookup_type("int"), form.as_integer());
}

IR_Value* Compiler::compile_symbol(const script::Object& form, Env* env) {
    std::string name = form.as_symbol().c_str();
    IR_Value* val = env->lookup(name);
    if (!val) {
        lg::error("Undefined variable: {}", name);
    }
    return val;
}

IR_Value* Compiler::compile_call(const script::Object& form, Env* env) {
    (void)env;
    lg::warn("Call not implemented: {}", form.print());
    return nullptr;
}


} // namespace sootc