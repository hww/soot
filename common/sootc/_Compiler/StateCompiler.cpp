// common/sootc/Compiler/StateCompiler.cpp
#include "common/sootc/Compiler/StateCompiler.hpp"
#include "file/ProgramBinaryElement.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Env/StateEnv.hpp"
#include "sootc/Env/TypeEnv.hpp"
#include "common/util/Log.hpp"
#include "type_system/TypeSpec.hpp"
#include <stdexcept>

namespace sootc {

StateCompiler::StateCompiler(TypeSystem& ts, Compiler* compiler) 
    : ts_(ts), compiler_(compiler) {}

// ============================================================================
// extract_state_name
// ============================================================================

std::string StateCompiler::extract_state_name(const soot::Object& form, const soot::Object& rest) {
    (void)rest;
    // form = (defstate idle (vector3) ...)
    auto pair = form.as_pair();
    if (pair) {
        auto cdr = pair->cdr;
        if (cdr.is_pair()) {
            auto name_obj = cdr.as_pair()->car;
            if (name_obj.is_symbol()) {
                return name_obj.as_symbol().c_str();
            }
        }
    }
    return "unknown-state";
}

// ============================================================================
// extract_parent_name
// ============================================================================

std::string StateCompiler::extract_parent_name(const soot::Object& form, const soot::Object& rest) {
    (void)rest;
    // form = (defstate idle (vector3) ...)
    //                      ^^^^^^^^ третий элемент
    auto pair = form.as_pair();
    if (pair) {
        auto cdr = pair->cdr;                    // (idle (vector3) ...)
        if (cdr.is_pair()) {
            auto cddr = cdr.as_pair()->cdr;      // ((vector3) ...)
            if (cddr.is_pair()) {
                auto parent_obj = cddr.as_pair()->car;
                if (parent_obj.is_pair() && parent_obj.as_pair()->car.is_symbol()) {
                    return parent_obj.as_pair()->car.as_symbol().c_str();
                }
                if (parent_obj.is_symbol()) {
                    return parent_obj.as_symbol().c_str();
                }
            }
        }
    }
    return "basic";
}

// ============================================================================
// extract_handlers
// ============================================================================

StateCompiler::HandlerForms StateCompiler::extract_handlers(const soot::Object& form, const soot::Object& rest) {
    HandlerForms handlers;
    // :rest = (name (type) :virtual #t :code ....)
    auto current = rest;
    if (!current.is_pair()) {
        throw std::runtime_error(fmt::format("Unexpected {} in {}", current.print(), form.print()));   
    }
    // Take rest of rest 
    current = current.as_pair()->cdr;
    // :rest = ((type) :virtual #t :code ....)
    if (!current.is_pair()) {
        throw std::runtime_error(fmt::format("Unexpected {} in {}", current.print(), form.print()));   
    }
    current = current.as_pair()->cdr;
    // :rest = (:virtual #t :code ....)
    while (current.is_pair()) {
        auto item = current.as_pair()->car;
        auto next = current.as_pair()->cdr;
        if (!next.is_pair()) {
            throw std::runtime_error(fmt::format("Unexpected {} {} in {}", item.print(), next.print(), form.print()));   
        }
        auto value = next.as_pair()->car;
        if (item.is_symbol()) {
            std::string keyword = item.as_symbol().c_str();
            
            if (keyword == ":virtual") {
                // :virtual #t или :virtual #f
                handlers.is_virtual = value.is_true();
            }
            else if (keyword == ":event") {
                handlers.event = value;
            }
            else if (keyword == ":enter") {
                handlers.enter = value;
            }
            else if (keyword == ":exit") {
                handlers.exit = value;
            }
            else if (keyword == ":code") {
                handlers.code = value;
            }
            else if (keyword == ":post") {
                handlers.post = value;
            }
            else if (keyword == ":trans") {
                handlers.trans = value;
            } else {
                throw std::runtime_error(fmt::format("Unexpected state keyword '{}' in form {}", keyword.c_str(), form.print()));
            }
        } else {
            throw std::runtime_error(fmt::format("Unexpected {} in {}", current.print(), form.print()));   
        }
        
        current = next.as_pair()->cdr;
    }
    
    return handlers;
}

// ============================================================================
// compile_handler
// ============================================================================

void StateCompiler::compile_handler(const soot::Object& handler_form, 
                                     const std::string& handler_name,
                                     StateEnv* s_env, 
                                     MethodEnv*& out_method_env) {
    if (handler_form.is_null() || !handler_form.is_true()) {
        out_method_env = nullptr;
        return;
    }
    
    // Обработчик — это behavior (лямбда с аргументами)
    // Пример: (behavior ((proc process) (argc int) (message symbol) (block event-message-block)) ...)
    
    if (!handler_form.is_pair()) {
        lg::error("Invalid handler form for '{}'", handler_name);
        out_method_env = nullptr;
        return;
    }
    
    // Парсим behavior
    auto behavior_form = handler_form;
    auto args_list = behavior_form.as_pair()->car;
    auto body_forms = behavior_form.as_pair()->cdr;
    
    // Создаем MethodEnv для обработчика
    Type* obj_type = ts_.lookup_type("object");
    auto* m_env = new MethodEnv(0, handler_name, s_env, obj_type);
    
    // Парсим аргументы
    auto current_arg = args_list;
    int arg_idx = 0;
    while (current_arg.is_pair()) {
        auto arg_decl = current_arg.as_pair()->car;
        if (arg_decl.is_pair()) {
            std::string arg_name = arg_decl.as_pair()->car.as_symbol().c_str();
            std::string type_name = arg_decl.as_pair()->cdr.as_pair()->car.as_symbol().c_str();
            Type* arg_type = ts_.lookup_type(type_name);
            m_env->define_argument(arg_name, arg_type, arg_idx++);
        }
        current_arg = current_arg.as_pair()->cdr;
    }
    
    // Компилируем тело
    auto current_body = body_forms;
    IR_Value* last_val = nullptr;
    while (current_body.is_pair()) {
        last_val = compiler_->compile(current_body.as_pair()->car, m_env);
        current_body = current_body.as_pair()->cdr;
    }
    
    if (last_val) {
        m_env->emit(soot::Object(), std::make_unique<IR_Return>(last_val));
    }
    
    out_method_env = m_env;
}

void StateCompiler::compile_handler_with_signature(
    const soot::Object& handler_form,
    const std::string& handler_name,
    StateEnv* s_env,
    const TypeSpec& expected_signature,
    MethodEnv*& out_method_env) {
    
    if (handler_form.is_none()) { // или !handler_form.is_true() как в твоем оригинале
        out_method_env = nullptr;
        return;
    }
    
    // 1. Создаем Env. Важно: берем тип владельца стейта
    Type* owner_type = const_cast<Type*>(s_env->type());
    auto* m_env = new MethodEnv(0, handler_name, s_env, owner_type);
    
    // 2. Устанавливаем ту самую сигнатуру, которая была в твоем старом коде
    m_env->method_function_type = expected_signature; 

    // 3. Регистрируем аргументы
    // Индекс 0 — это ВСЕГДА self (твой старый код его пропускал)
    m_env->define_argument("self", owner_type, 0);
    
    // Остальные аргументы из сигнатуры (например, position) пойдут с индекса 1
    int arg_idx = 1;
    size_t num_args = expected_signature.get_args_count();
    
    // Проходим по аргументам сигнатуры (минус возвращаемое значение)
    for (size_t i = 0; i < num_args - 1; i++) {
        const TypeSpec& param_spec = expected_signature.get_arg(i);
        Type* arg_type = ts_.lookup_type(param_spec);
        
        // Именуем arg1, arg2... (или можно вытянуть имя из формы, если нужно)
        std::string arg_name = fmt::format("arg{}", arg_idx);
        m_env->define_argument(arg_name, arg_type, arg_idx++);
    }
    
    // 4. Компилируем тело
    FunctionCompiler func_compiler(ts_, compiler_);
    
    // Если форма — это список (body...), компилируем его через наш новый унифицированный метод
    // Если это (function (args) body...), нужно взять cdr->cdr
    soot::Object body = handler_form;
    if (handler_form.is_pair() && handler_form.as_pair()->car.is_symbol() &&
        handler_form.as_pair()->car.as_symbol().c_str() == std::string("function")) {
        body = handler_form.as_pair()->cdr.as_pair()->cdr;
    }
    
    func_compiler.compile_body(body, m_env);
    
    out_method_env = m_env;
}

// ============================================================================
// compile
// ============================================================================

IR_Value* StateCompiler::compile(const soot::Object& form, 
                                  const soot::Object& rest, 
                                  Env* env) {
    std::string state_name = extract_state_name(form, rest);
    std::string parent_name = extract_parent_name(form, rest);  
    auto handlers = extract_handlers(form, rest);
    
    // Находим TypeEnv родителя
    IR_Value* parent_val = env->lookup(parent_name);
    if (!parent_val) {
        throw std::runtime_error(fmt::format("Undefined type '{}'", parent_name));
        return nullptr;
    }
    
    auto* parent_ir_type = dynamic_cast<IR_Type*>(parent_val);
    if (!parent_ir_type) {
         throw std::runtime_error(fmt::format("'{}' is not a type", parent_name));
        return nullptr;
    }
    
    TypeEnv* parent_type_env = parent_ir_type->get_env();
    Type* parent_type = parent_type_env->get_type();
    
    // Создаем StateEnv
    auto* s_env = parent_type_env->find_state(state_name);
    if (s_env == nullptr) {
        throw std::runtime_error(fmt::format("Type environment '{}' does nop have state '{}'", parent_name, state_name));
        return nullptr;   
    }

    s_env->set_is_virtual(handlers.is_virtual);
    s_env->set_defined(true);

    // 1. Инициализируем указатели
    MethodEnv* event_method = nullptr;
    MethodEnv* enter_method = nullptr;
    MethodEnv* exit_method = nullptr;
    MethodEnv* code_method = nullptr;
    MethodEnv* post_method = nullptr;
    MethodEnv* trans_method = nullptr;

    // 2. Компилируем :code с учетом объявленной сигнатуры (аргументы из defstate)
    TypeSpec declared_signature = s_env->type_spec();
    if (!handlers.code.is_none()) {
        compile_handler_with_signature(handlers.code, "code", s_env, 
                                    declared_signature, code_method);
    }

    // 3. Теперь валидация :code имеет смысл, так как code_method уже может быть не null
    if (code_method) {
        TypeSpec implemented_type = code_method->method_function_type;
        if (!ts_.tc(declared_signature, implemented_type)) {
            throw std::runtime_error(fmt::format(
                "State '{}' code signature mismatch.\n  Declared: {}\n  Implemented: {}",
                state_name, declared_signature.print(), implemented_type.print()));
        }
    }

    // 4. Компилируем остальные хендлеры (у них обычно фиксированные сигнатуры или behavior)
    if (!handlers.event.is_none()) 
        compile_handler(handlers.event, "event", s_env, event_method);

    if (!handlers.enter.is_none())
        compile_handler(handlers.enter, "enter", s_env, enter_method);

    if (!handlers.exit.is_none())
        compile_handler(handlers.exit, "exit", s_env, exit_method);

    if (!handlers.post.is_none())
        compile_handler(handlers.post, "post", s_env, post_method);

    if (!handlers.trans.is_none())
        compile_handler(handlers.trans, "trans", s_env, trans_method);

    // 5. Записываем результат в окружение состояния
    s_env->set_event_method(event_method);
    s_env->set_enter_method(enter_method);
    s_env->set_exit_method(exit_method);
    s_env->set_code_method(code_method);
    s_env->set_post_method(post_method);
    s_env->set_trans_method(trans_method);

    return new IR_StateValue(s_env);
}

// ============================================================================
// build
// ============================================================================
ProgramBinaryElement StateCompiler::build(StateEnv* s_env) {
    // Make state name type_name::state_name
    std::string state_full_name = s_env->type_env()->name() + "::" + s_env->name();
    
    // 1. Создаем буферы для разных сегментов (по аналогии с эталоном)
    RelocatableBuffer result_state(state_full_name + "#descriptor", "state", true);
    RelocatableBuffer result_defs(state_full_name + "#definitions", "defs", true);
    RelocatableBuffer result_bodies(state_full_name + "#bodies", "code", true);

    // Подготавливаем список хендлеров
    std::vector<std::pair<int, MethodEnv*>> handlers = {
        {StateDesc::CODE_ID,  s_env->code_method()},
        {StateDesc::ENTER_ID, s_env->enter_method()},
        {StateDesc::EXIT_ID,  s_env->exit_method()},
        {StateDesc::TRANS_ID, s_env->trans_method()},
        {StateDesc::POST_ID,  s_env->post_method()},
        {StateDesc::EVENT_ID, s_env->event_method()},
    };

    // 2. Сборка таблицы Definitions и тел методов
    for (auto& [id, method] : handlers) {
        Definition def{};
        
        if (method) {
            def.name = StringId(method->get_name());
            def.type = StringId("function");
            
            // Метка для FunctionDesc этого метода
            // Make state name type_name::state_name::method_name
            std::string method_desc_label = state_full_name + "::" + method->get_name();;

            // Definition.ptr должен указывать на FunctionDesc (которую вернет MethodCompiler)
            result_defs.add_relocatable(
                result_defs.size() + offsetof(Definition, ptr), 
                Relocation::Type::LABEL_ADDRESS, 
                method_desc_label + "#descriptor"
            );

            // Компилируем само тело (FunctionDesc + Bytecode)
            FunctionCompiler method_compiler(ts_, compiler_);
            RelocatableBuffer method_res = method_compiler.build(method, method_desc_label);
            
            // Добавляем скомпилированный метод в сегмент тел
            result_bodies.add_buffer(method_res);
        }

        // Записываем структуру Definition в таблицу
        result_defs.add_bytes(&def, sizeof(Definition));
    }

    // 3. Формируем заголовок StateDesc
    StateDesc desc{};
    desc.name = StringId(s_env->name());
    desc.parent_state = StringId(s_env->type()->name());
    desc.defs_count = (u32)handlers.size();
    desc.flags = s_env->is_virtual() ? StateFlags::Virtual : StateFlags::None;
    desc.definitions = nullptr; 

    // Релокация: StateDesc::definitions указывает на начало сегмента result_defs
    result_state.add_relocatable(
        offsetof(StateDesc, definitions),
        Relocation::Type::LABEL_ADDRESS, 
        result_defs.name()
    );

    result_state.add_bytes(&desc, sizeof(StateDesc));

    // 4. Склеиваем всё в один итоговый буфер в правильном порядке
    // Сначала идет дескриптор состояния, потом таблица определений, потом сами функции
    result_state.add_buffer(result_defs);
    result_state.add_buffer(result_bodies);

    return result_state;
}
} // namespace sootc