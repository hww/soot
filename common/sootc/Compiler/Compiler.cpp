#include "Compiler.hpp"
#include "common/util/Log.hpp"
#include "files/BinaryFile.hpp"
#include "files/BinaryFileBuilder.hpp"
#include "files/Definition.hpp"
#include "files/RelocatableBuffer.hpp"
#include "lib/StringId.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Compiler/MethodCompiler.hpp"
#include "sootc/Compiler/TypeCompiler.hpp"
#include "sootc/Env/FileEnv.hpp"
#include "sootc/IR/IR_Node.hpp"
#include "sootc/Env/Export.hpp"
#include "common/carbon/files/FunctionDesc.hpp"
#include "sootc/IR/IR_Value.hpp"
#include <cstddef>

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
    REGISTER_FORM("defmethod", compile_defmethod);
    REGISTER_FORM("deftype", compile_deftype);
    REGISTER_FORM("+", compile_add);
    REGISTER_FORM("-", compile_sub);
}
#undef REGISTER_FORM

// ============================================================================
// Единый метод компиляции
// ============================================================================

IR_Value* Compiler::compile(const script::Object& form, Env* env) {
    set_current_env(env);
    
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
// compile_module — один проход компиляции, потом сборка
// ============================================================================
std::shared_ptr<carbon::modules::Module> Compiler::compile_module(const script::Object& forms, FileEnv* env) {
    
    // ПРОХОД 1: Компиляция всех форм в IR_Value
    auto current = forms;
    while (current.is_pair()) {
        compile(current.as_pair()->car, env);
        current = current.as_pair()->cdr;
    }
    
    // ПРОХОД 2: BUILD — генерация буферов
    RelocatableBuffer definitions;
    RelocatableBuffer descriptors;
    int definitions_count = 0;

    for (auto& [name, value] : env->sybols_table()) {
        lg::info("Compiler compile symbol '{}'", name);
        if (auto result = value->build(this)) {            
            const auto& [type_tag, buffer] = *result;

            lg::info("  > add_definition compile symbol '{}' type '{}'", name, type_tag);

            // Метка для дефиниции
            definitions.add_label(name);
            
            // Заголовок дефиниции
            Definition definition{
                StringId(name),
                StringId(type_tag),
                SymbolFlags::Export,
                0,
                Ptr<u8>()
            };
            
            // Ссылка на дескриптор
            std::string descriptor_label = name + "#desc";
            definitions.add_relocatable(offsetof(Definition, ptr), 
                                        Relocation::Type::LABEL_ADDRESS, 
                                        descriptor_label);
            definitions.add_bytes(&definition, sizeof(Definition));
            
            // Дескриптор
            descriptors.add_label(descriptor_label);
            descriptors.add_buffer(buffer);
            
            definitions_count++;
        } 
    }
    
    // Создаем заголовок
    RelocatableBuffer file_buffer;
    
    // Заглушка для BinaryFile (заполним позже)
    u32 header_pos = file_buffer.size();
    BinaryFile dummy_header{};
    file_buffer.add_bytes(&dummy_header, sizeof(BinaryFile));
    
    // Добавляем таблицу дефиниций
    u32 definitions_offset = file_buffer.size();
    file_buffer.add_buffer(definitions);
    
    // Добавляем дескрипторы
    u32 descriptors_offset = file_buffer.size();
    file_buffer.add_buffer(descriptors);
    
    // Теперь патчим заголовок
    BinaryFile header{};
    header.base_offset = 0;
    header.magic = BinaryFile::MAGIC;
    header.generation = BinaryFile::CURRENT_GENERATION;
    header.definitions_count = definitions_count;
    header.definitions.offset = definitions_offset;
    header.file_size = file_buffer.size();
    header.used_size = file_buffer.size();
    
    // Записываем заголовок поверх заглушки
    file_buffer.write_at(header_pos, &header, sizeof(BinaryFile));
    
    // Линковка
    auto file = file_buffer.link_file();
    
    // Создаем модуль
    auto module = std::make_shared<Module>();
    file->relocate_pointers(true, module.get());
    module->name = StringId(env->name().c_str());
    module->set_file(file_buffer.bytes());
    
    return module;
}

// ============================================================================
// Обработчики форм
// ============================================================================

IR_Value* Compiler::compile_define(const script::Object&, const script::Object& rest, Env* env) {
    std::string name = rest.as_pair()->car.as_symbol().c_str();
    auto value_form = rest.as_pair()->cdr.as_pair()->car;

    IR_Value* val = compile(value_form, env);
    // For a function
    auto* function = reinterpret_cast<IR_FunctionValue*>(val);
    if (function) {
        function->get_env()->set_name(name);
    }
    env->bind(name, val);
    return val;
}

IR_Value* Compiler::compile_lambda(const script::Object& form, const script::Object& rest, Env* env) {
    FunctionCompiler func_compiler(ts_, this);
    return func_compiler.compile_function(form, rest, env);
}

IR_Value* Compiler::compile_begin(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;
    auto current = rest;
    IR_Value* last = nullptr;
    while (current.is_pair()) {
        last = compile(current.as_pair()->car, env);
        current = current.as_pair()->cdr;
    }
    return last;
}

IR_Value* Compiler::compile_defmethod(const script::Object& form, const script::Object& rest, Env* env) {
    MethodCompiler method_compiler(ts_, this);
    return method_compiler.compile(form, rest, env);
}

IR_Value* Compiler::compile_deftype(const script::Object& form, const script::Object& rest, Env* env) {
    TypeCompiler type_compiler(ts_, this);
    return type_compiler.compile(form, rest, env);
}

// ============================================================================
// Арифметика
// ============================================================================

IR_Value* Compiler::compile_add(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;
    auto args = rest;
    IR_Value* v_left = compile(args.as_pair()->car, env);
    IR_Value* v_right = compile(args.as_pair()->cdr.as_pair()->car, env);

    IR_Reg* r_left = v_left->to_reg(*env);
    IR_Reg* r_right = v_right->to_reg(*env);

    IR_Reg* dest = env->function_env()->alloc_reg(r_left->get_type());
    env->emit(script::Object(), std::make_unique<IR_Binary>(IR_Binary::Op::ADD, dest, r_left, r_right));

    return dest;
}

IR_Value* Compiler::compile_sub(const script::Object& form, const script::Object& rest, Env* env) {
    (void)form;
    auto args = rest;
    IR_Value* v_left = compile(args.as_pair()->car, env);
    IR_Value* v_right = compile(args.as_pair()->cdr.as_pair()->car, env);

    IR_Reg* r_left = v_left->to_reg(*env);
    IR_Reg* r_right = v_right->to_reg(*env);

    IR_Reg* dest = env->function_env()->alloc_reg(r_left->get_type());
    env->emit(script::Object(), std::make_unique<IR_Binary>(IR_Binary::Op::SUB, dest, r_left, r_right));

    return dest;
}

// ============================================================================
// Атомы и вызовы
// ============================================================================

IR_Value* Compiler::compile_number(const script::Object& form, Env*) {
    return new IR_Const(ts_.lookup_type("int"), form.as_integer());
}

IR_Value* Compiler::compile_symbol(const script::Object& form, Env* env) {
    std::string name = form.as_symbol().c_str();
    
    // Keyword (начинается с ':') — самовычисляемая константа
    if (!name.empty() && name[0] == ':') {
        // Создаем константу типа keyword
        Type* keyword_type = ts_.lookup_type("symbol");
        auto string_id = static_cast<s64>(StringId(name));
        // TODO: нужно создать IR_Const, который хранит StringId
        return new IR_Const(keyword_type,string_id); // placeholder
    }
    
    // Обычный символ — ищем в окружении
    IR_Value* val = env->lookup(name);
    if (!val) {
        lg::error("Undefined variable: {}", name);
    }
    return val;
}

IR_Value* Compiler::compile_call(const script::Object& form, Env* env) {
    auto pair = form.as_pair();
    IR_Value* callee = compile(pair->car, env);
    
    std::vector<IR_Value*> args;
    auto current = pair->cdr;
    while (current.is_pair()) {
        args.push_back(compile(current.as_pair()->car, env));
        current = current.as_pair()->cdr;
    }
    
    Type* obj_type = ts_.lookup_type("object");
    IR_Reg* result = env->function_env()->alloc_reg(obj_type);
    
    env->emit(form, std::make_unique<IR_Call>(result, callee, nullptr, args));
    
    return result;
}

} // namespace sootc