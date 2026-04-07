// common/sootc/Compiler/StateCompiler.cpp
#include "common/sootc/Compiler/StateCompiler.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Env/StateEnv.hpp"
#include "sootc/Env/TypeEnv.hpp"
#include "common/util/Log.hpp"

namespace sootc {

StateCompiler::StateCompiler(TypeSystem& ts, Compiler* compiler) 
    : ts_(ts), compiler_(compiler) {}

// ============================================================================
// extract_state_name
// ============================================================================

std::string StateCompiler::extract_state_name(const script::Object& form) {
    // form = (defstate name ...)
    auto pair = form.as_pair();
    if (pair && pair->cdr.is_pair()) {
        auto second = pair->cdr.as_pair()->car;
        if (second.is_symbol()) {
            return second.as_symbol().c_str();
        }
    }
    return "unknown-state";
}

// ============================================================================
// extract_parent_name
// ============================================================================

std::string StateCompiler::extract_parent_name(const script::Object& rest) {
    // rest = (parent) ...
    auto first = rest.as_pair()->car;
    if (first.is_pair() && first.as_pair()->car.is_symbol()) {
        return first.as_pair()->car.as_symbol().c_str();
    }
    return "process";  // базовый тип по умолчанию
}

// ============================================================================
// extract_handlers
// ============================================================================

StateCompiler::HandlerForms StateCompiler::extract_handlers(const script::Object& rest) {
    HandlerForms handlers;
    
    auto current = rest;
    while (current.is_pair()) {
        auto item = current.as_pair()->car;
        if (item.is_pair() && item.as_pair()->car.is_symbol()) {
            std::string keyword = item.as_pair()->car.as_symbol().c_str();
            
            if (keyword == ":virtual") {
                // :virtual #t или :virtual #f
                auto value = item.as_pair()->cdr.as_pair()->car;
                handlers.is_virtual = value.is_symbol() && value.as_symbol().c_str() == std::string("#t");
            }
            else if (keyword == ":event") {
                handlers.event = item.as_pair()->cdr.as_pair()->car;
            }
            else if (keyword == ":enter") {
                handlers.enter = item.as_pair()->cdr.as_pair()->car;
            }
            else if (keyword == ":exit") {
                handlers.exit = item.as_pair()->cdr.as_pair()->car;
            }
            else if (keyword == ":code") {
                handlers.code = item.as_pair()->cdr.as_pair()->car;
            }
            else if (keyword == ":post") {
                handlers.post = item.as_pair()->cdr.as_pair()->car;
            }
            else if (keyword == ":trans") {
                handlers.trans = item.as_pair()->cdr.as_pair()->car;
            }
        }
        current = current.as_pair()->cdr;
    }
    
    return handlers;
}

// ============================================================================
// compile_handler
// ============================================================================

void StateCompiler::compile_handler(const script::Object& handler_form, 
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
        m_env->emit(script::Object(), std::make_unique<IR_Return>(last_val));
    }
    
    out_method_env = m_env;
}

// ============================================================================
// compile
// ============================================================================

IR_Value* StateCompiler::compile(const script::Object& form, 
                                  const script::Object& rest, 
                                  Env* env) {
    std::string state_name = extract_state_name(form);
    std::string parent_name = extract_parent_name(rest);
    auto handlers = extract_handlers(rest);
    
    // Находим TypeEnv родителя
    IR_Value* parent_val = env->lookup(parent_name);
    if (!parent_val) {
        lg::error("Parent type '{}' not found for state '{}'", parent_name, state_name);
        return nullptr;
    }
    
    auto* parent_ir_type = dynamic_cast<IR_Type*>(parent_val);
    if (!parent_ir_type) {
        lg::error("'{}' is not a type", parent_name);
        return nullptr;
    }
    
    TypeEnv* parent_type_env = parent_ir_type->get_env();
    Type* parent_type = parent_type_env->get_type();
    
    // Создаем StateEnv
    auto* s_env = new StateEnv(state_name, parent_type_env, parent_type, parent_type_env);
    s_env->set_is_virtual(handlers.is_virtual);
    
    // Компилируем обработчики
    MethodEnv* event_method = nullptr;
    MethodEnv* enter_method = nullptr;
    MethodEnv* exit_method = nullptr;
    MethodEnv* code_method = nullptr;
    MethodEnv* post_method = nullptr;
    MethodEnv* trans_method = nullptr;
    
    compile_handler(handlers.event, "event", s_env, event_method);
    compile_handler(handlers.enter, "enter", s_env, enter_method);
    compile_handler(handlers.exit, "exit", s_env, exit_method);
    compile_handler(handlers.code, "code", s_env, code_method);
    compile_handler(handlers.post, "post", s_env, post_method);
    compile_handler(handlers.trans, "trans", s_env, trans_method);
    
    s_env->set_event_method(event_method);
    s_env->set_enter_method(enter_method);
    s_env->set_exit_method(exit_method);
    s_env->set_code_method(code_method);
    s_env->set_post_method(post_method);
    s_env->set_trans_method(trans_method);
    
    // Регистрируем состояние в типе
    parent_type_env->bind(state_name, new IR_StateValue(s_env));
    
    return new IR_StateValue(s_env);
}

// ============================================================================
// build
// ============================================================================

RelocatableBuffer StateCompiler::build(StateEnv* s_env) {
    RelocatableBuffer buffer(s_env->name(),"state");
    
    // 1. Создаем массив Definition для обработчиков
    std::vector<Definition> definitions;
    std::vector<std::pair<int, MethodEnv*>> handlers = {
        {StateDesc::CODE_ID, s_env->code_method()},
        {StateDesc::ENTER_ID, s_env->enter_method()},
        {StateDesc::EXIT_ID, s_env->exit_method()},
        {StateDesc::TRANS_ID, s_env->trans_method()},
        {StateDesc::POST_ID, s_env->post_method()},
        {StateDesc::EVENT_ID, s_env->event_method()},
    };
    
    for (auto& [id, method] : handlers) {
        if (method) {
            Definition def{};
            def.name = StringId(method->name());
            def.type = StringId("function");
            def.flags = SymbolFlags::None;
            def.ptr = Ptr<u8>();  // будет заполнено при линковке
            
            // Добавляем релокацию на код метода
            std::string method_symbol = s_env->type_env()->name() + "::" + method->name();
            buffer.add_relocatable(offsetof(Definition, ptr), 
                                   Relocation::Type::LABEL_ADDRESS, 
                                   method_symbol + "#code");
            buffer.add_bytes(&def, sizeof(Definition));
        } else {
            // Пустая заглушка
            Definition def{};
            buffer.add_bytes(&def, sizeof(Definition));
        }
    }
    
    // 2. Заголовок StateDesc
    StateDesc desc{};
    desc.name = StringId(s_env->name());
    desc.parent_state = StringId(s_env->type()->name());  // нужно добавить метод
    desc.defs_count = definitions.size();
    desc.flags = s_env->is_virtual() ? StateFlags::Virtual : StateFlags::None;
    
    // 3. Записываем StateDesc и таблицу определений
    u32 desc_start = buffer.size();
    buffer.add_bytes(&desc, sizeof(StateDesc));
    
    // Патчим указатель на таблицу определений
    u32 definitions_offset = buffer.size();
    buffer.add_relocatable(offsetof(StateDesc, definitions), 
                           Relocation::Type::FIXED_ADDRESS, "");
    
    return buffer;
}

} // namespace sootc