#include "Compiler.hpp"
#include "common/util/Log.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Compiler/MethodCompiler.hpp"
#include "sootc/Compiler/TypeCompiler.hpp"
#include "sootc/IR/IR_Node.hpp"
#include "sootc/Env/Export.hpp"
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
    // DECLARE фаза
    REGISTER_FORM("define", compile_define);
    REGISTER_FORM("lambda", compile_lambda);
    REGISTER_FORM("begin",  compile_begin);
    REGISTER_FORM("defmethod", compile_defmethod);
    REGISTER_FORM("deftype", compile_deftype);
    
    // RESOLVE фаза
    REGISTER_FORM("define-resolve", resolve_define);
    REGISTER_FORM("lambda-resolve", resolve_lambda);
    REGISTER_FORM("begin-resolve", resolve_begin);
    REGISTER_FORM("defmethod-resolve", resolve_defmethod);
    REGISTER_FORM("deftype-resolve", resolve_deftype);
    
    // Арифметика (работает в обеих фазах одинаково)
    REGISTER_FORM("+", compile_add);
    REGISTER_FORM("-", compile_sub);
}
#undef REGISTER_FORM

// ============================================================================
// ФАЗА 1: DECLARE
// ============================================================================

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

// ============================================================================
// ФАЗА 2: RESOLVE
// ============================================================================

IR_Value* Compiler::resolve(const script::Object& form, Env* env) {
    set_current_env(env);
    
    if (form.is_number()) return compile_number(form, env);
    if (form.is_symbol()) return compile_symbol(form, env);

    if (form.is_pair()) {
        auto pair = form.as_pair();
        if (pair->car.is_symbol()) {
            std::string op = pair->car.as_symbol().c_str();
            
            // Для спецформ используем resolve-версию
            if (op == "define") {
                return resolve_define(form, pair->cdr, env);
            }
            if (op == "lambda") {
                return resolve_lambda(form, pair->cdr, env);
            }
            if (op == "begin") {
                return resolve_begin(form, pair->cdr, env);
            }
            if (op == "defmethod") {
                return resolve_defmethod(form, pair->cdr, env);
            }
            if (op == "deftype") {
                return resolve_deftype(form, pair->cdr, env);
            }
            
            // Арифметика
            if (op == "+" || op == "-") {
                auto it = m_forms.find(op);
                if (it != m_forms.end()) {
                    return it->second(this, form, pair->cdr, env);
                }
            }
        }
        return resolve_call(form, env);
    }
    return nullptr;
}

// ============================================================================
// RESOLVE: Спецформы
// ============================================================================

IR_Value* Compiler::resolve_define(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;
    std::string name = rest.as_pair()->car.as_symbol().c_str();
    auto value_form = rest.as_pair()->cdr.as_pair()->car;

    IR_Value* val = resolve(value_form, env);
    env->bind(name, val);
    return val;
}

IR_Value* Compiler::resolve_lambda(const script::Object& form, const script::Object& rest, Env* env) {
    FunctionCompiler func_compiler(ts_, this);
    IR_Value* f_val = func_compiler.declare_function(form, rest, env);
    func_compiler.resolve_body(static_cast<IR_FunctionValue*>(f_val)->get_env());
    return f_val;
}

IR_Value* Compiler::resolve_begin(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;
    auto current = rest;
    IR_Value* last = nullptr;
    while (current.is_pair()) {
        last = resolve(current.as_pair()->car, env);
        current = current.as_pair()->cdr;
    }
    return last;
}

IR_Value* Compiler::resolve_defmethod(const script::Object& form, const script::Object& rest, Env* env) {
    MethodCompiler method_compiler(ts_, this);
    IR_Value* m_val = method_compiler.declare(form, rest, env);
    method_compiler.resolve(static_cast<IR_MethodValue*>(m_val)->get_env());
    return m_val;  // ← добавлено
}

IR_Value* Compiler::resolve_deftype(const script::Object& form, const script::Object& rest, Env* env) {
    TypeCompiler type_compiler(ts_, this);
    IR_Value* t_val = type_compiler.declare(form, rest, env);
    type_compiler.resolve(static_cast<IR_Type*>(t_val)->get_env());
    return t_val;  // ← добавлено
}

IR_Value* Compiler::resolve_call(const script::Object& form, Env* env) {
    // Для вызова функции в RESOLVE фазе
    auto pair = form.as_pair();
    IR_Value* callee = resolve(pair->car, env);
    
    std::vector<IR_Value*> args;
    auto current = pair->cdr;
    while (current.is_pair()) {
        args.push_back(resolve(current.as_pair()->car, env));
        current = current.as_pair()->cdr;
    }
    
    // Создаем IR_Call
    Type* obj_type = ts_.lookup_type("object");
    IR_Reg* result = env->function_env()->alloc_reg(obj_type);
    
    env->emit(form, std::make_unique<IR_Call>(result, callee, nullptr, args));
    
    return result;
}

// ============================================================================
// Атомы и вызовы (DECLARE фаза)
// ============================================================================

IR_Value* Compiler::compile_number(const script::Object& form, Env*) {
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
    // В DECLARE фазе вызовы не обрабатываются глубоко
    // Только создаем заглушку
    (void)env;
    lg::warn("Call not implemented in DECLARE phase: {}", form.print());
    return nullptr;
}

// ============================================================================
// Арифметика (работает в обеих фазах)
// ============================================================================

IR_Value* Compiler::compile_add(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;
    auto args = rest;
    IR_Value* v_left = declare(args.as_pair()->car, env);
    IR_Value* v_right = declare(args.as_pair()->cdr.as_pair()->car, env);

    IR_Reg* r_left = v_left->to_reg(static_cast<Env&>(*env));
    IR_Reg* r_right = v_right->to_reg(static_cast<Env&>(*env));

    IR_Reg* dest = env->function_env()->alloc_reg(r_left->get_type());
    env->emit(script::Object(), std::make_unique<IR_Binary>(IR_Binary::Op::ADD, dest, r_left, r_right));

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
    env->emit(script::Object(), std::make_unique<IR_Binary>(IR_Binary::Op::SUB, dest, r_left, r_right));

    return dest;
}

// ============================================================================
// Остальные DECLARE обработчики
// ============================================================================

IR_Value* Compiler::compile_define(const script::Object&, const script::Object& rest, Env* env) {
    std::string name = rest.as_pair()->car.as_symbol().c_str();
    auto value_form = rest.as_pair()->cdr.as_pair()->car;

    IR_Value* val = declare(value_form, env);
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
    return func_compiler.declare_function(form, rest, env);
}

IR_Value* Compiler::compile_defmethod(const script::Object& form, const script::Object& rest, Env* env) {
    MethodCompiler method_compiler(ts_, this);
    return method_compiler.declare(form, rest, env);
}

IR_Value* Compiler::compile_deftype(const script::Object& form, const script::Object& rest, Env* env) {
    TypeCompiler type_compiler(ts_, this);
    return type_compiler.declare(form, rest, env);
}

// ============================================================================
// Хелперы
// ============================================================================

TypeSpec Compiler::build_typespec_from_env(FunctionEnv* env) {
    const auto& params = env->params();
    
    int max_idx = env->get_max_param_index();

    std::vector<TypeSpec> ordered_args;
    ordered_args.resize(max_idx + 1, TypeSpec("object"));

    for (auto* val : params) {
        if (auto* reg = dynamic_cast<IR_Reg*>(val)) {
            int idx = reg->get_index();
            if (idx >= 0) {
                ordered_args[idx] = TypeSpec(reg->get_type()->get_name());
            }
        }
    }

    std::vector<TypeSpec> typespec_args;
    for (const auto& ts_arg : ordered_args) {
        typespec_args.push_back(ts_arg);
    }
    
    Type* ret_type = env->get_return_type();
    if (ret_type) {
        typespec_args.push_back(TypeSpec(ret_type->get_name()));
    } else {
        typespec_args.push_back(TypeSpec("none")); 
    }

    return TypeSpec("function", std::move(typespec_args));
}

void Compiler::error_in_macro(const script::Object& form, const std::string& msg) {
    (void)form;  // ← подавить warning
    auto* macro_env = current_env()->macro_expand_env();
    if (macro_env) {
        // auto& original = macro_env->root_form();  // ← закомментировать или удалить если не используется
        throw std::runtime_error(fmt::format("{} (from macro {})", 
                           msg, macro_env->macro_name().c_str()));
    }
    throw std::runtime_error(fmt::format("{}", msg));
}
// ============================================================================
// compile_module — обновленная версия с resolve
// ============================================================================

std::shared_ptr<carbon::modules::Module> Compiler::compile_module(const script::Object& forms, Env* env) {
    
    // ФАЗА 1: DECLARE
    auto current = forms;
    while (current.is_pair()) {
        declare(current.as_pair()->car, env); 
        current = current.as_pair()->cdr;
    }
    
    // Собираем список всех значений
    std::vector<std::pair<std::string, IR_Value*>> work_list;
    for (auto& value : env->symbols()) {
        auto name = env->get_value_name(value);
        work_list.push_back({name, value});
    }

    // ФАЗА 2: RESOLVE — теперь resolve рекурсивно компилирует тела
    for (auto& [name, value] : work_list) {
        // Вызываем resolve у IR_Value, который внутри вызовет Compiler::resolve
        // для своих дочерних форм
        value->resolve(this);
    }

    // ФАЗА 3: BUILD
    for (auto& [name, value] : work_list) {
        if (auto result = value->build(this)) {
            const auto& [type_tag, buffer] = *result;
            builder_.add_definition(name, type_tag, std::move(buffer));
        }
    }

    return builder_.build_module();
}

void Compiler::add_definition(const std::string& name, const std::string& type, 
                              carbon::files::RelocatableBuffer buffer, 
                              carbon::files::SymbolFlags flags) {
    builder_.add_definition(name, type, std::move(buffer), flags);
}

RelocatableBuffer Compiler::finalize_function(FunctionEnv* fe) {
    FunctionCompiler func_compiler(ts_, this);
    return func_compiler.build(fe);
}

} // namespace sootc