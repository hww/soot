#include "sootc/Compiler/MethodCompiler.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Compiler/Compiler.hpp"
#include "sootc/IR/IR_Value.hpp"
#include "common/util/Log.hpp"

namespace sootc {

MethodCompiler::MethodCompiler(TypeSystem& ts, Compiler* compiler) 
    : ts_(ts), compiler_(compiler) {}

IR_Value* MethodCompiler::declare(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;
    if (!rest.is_pair()) return nullptr;

    std::string method_name = rest.as_pair()->car.as_symbol().c_str();
    std::string owner_name = extract_owner_name(rest);

    Type* owner_type = ts_.lookup_type(owner_name);
    if (!owner_type) {
        lg::error("Type {} not found in TypeSystem", owner_name);
        return nullptr;
    }

    // Создаем окружение метода. 
    // Нам нужно сохранить 'rest' (тело), чтобы распарсить его позже в build
    auto* environment = new MethodEnv(env, method_name, owner_type);
    environment->set_source_form(rest); 

    auto* m_val = new IR_MethodValue(environment);
    
    // Регистрируем в TypeSystem (пока с пустым TypeSpec)
    ts_.define_method(owner_type, method_name, TypeSpec(), std::nullopt);

    return m_val;
}

RelocatableBuffer MethodCompiler::build(IR_MethodValue* m_val) {
    auto* m_env = m_val->get_env();
    auto rest = m_env->source_form();
    std::string method_name = m_val->name();

    FunctionCompiler fn_compiler(ts_, compiler_);

    // 1. Подготавливаем форму для FunctionCompiler
    // Для обычного метода rest: (name ((this T) args...) body...)
    // Нам нужно скормить FunctionCompiler только (args...) и body...
    script::Object fn_rest;
    if (method_name == "new") {
        // (new (args...) body...) -> rest.cdr это ((args...) body...)
        fn_rest = rest.as_pair()->cdr;
    } else {
        // (name ((this T) args...) body...) -> rest.cdr это (((this T) args...) body...)
        fn_rest = rest.as_pair()->cdr;
    }

    // 2. Вызываем declare у FunctionCompiler, чтобы создать скелет функции
    IR_Value* ir_func = fn_compiler.declare(script::Object::make_symbol("function"), fn_rest, m_env);
    if (!ir_func) return RelocatableBuffer();

    auto* f_val = static_cast<IR_FunctionValue*>(ir_func);

    // 3. ВТОРОЙ ПРОХОД: Компилируем тело функции
    fn_compiler.compile_body(f_val);

    // 4. Генерируем байткод функции
    RelocatableBuffer actual_code = fn_compiler.build(f_val->get_env());

    // 5. Формируем MethodDef
    carbon::files::MethodDef method_def{};
    method_def.name = StringId(method_name.c_str());
    // Тут можно добавить логику определения типа метода (CTOR, VIRTUAL и т.д.)

    return build_method_buffer(method_def, actual_code);
}

std::string MethodCompiler::extract_owner_name(const script::Object& rest) {
    auto method_name_str = std::string(rest.as_pair()->car.as_symbol().c_str());
    
    try {
        if (method_name_str == "new") {
            // (new T (args) ...)
            return rest.as_pair()->cdr.as_pair()->car.as_symbol().c_str();
        } else {
            // (name ((this T) args...) ...)
            auto args_list = rest.as_pair()->cdr.as_pair()->car;
            auto first_arg = args_list.as_pair()->car; // (this T)
            return first_arg.as_pair()->cdr.as_pair()->car.as_symbol().c_str();
        }
    } catch (...) {
        lg::error("Failed to extract owner name for method {}", method_name_str);
        return "object";
    }
}

RelocatableBuffer MethodCompiler::build_method_buffer(const carbon::files::MethodDef& method_def,
                                                   const RelocatableBuffer& func_buffer) {
    RelocatableBuffer buffer;
    
    // Резервируем место под заголовок метода
    u32 m_start = buffer.size();
    buffer.add_bytes(&method_def, sizeof(carbon::files::MethodDef));
    
    // Добавляем тело функции (байткод)
    u32 f_start = buffer.size();
    buffer.add_buffer(func_buffer);
    
    // Проставляем смещение к данным (data) в структуре MethodDef
    u32 offset = m_start + offsetof(carbon::files::MethodDef, data);
    
    if (buffer.size() >= offset + sizeof(u64)) {
        *reinterpret_cast<u64*>(buffer.data() + offset) = (u64)f_start;
    }
    
    buffer.add_relocatable_offset(offset);
    
    return buffer;
}

} // namespace sootc