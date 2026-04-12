#include "Compiler.hpp"
#include "common/util/Log.hpp"
#include "common/util/FileUtil.hpp"
#include "files/BinaryFile.hpp"
#include "files/BinaryFileBuilder.hpp"
#include "files/Definition.hpp"
#include "files/RelocatableBuffer.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include "lib/StringId.hpp"
#include "sootc/Compiler/FunctionCompiler.hpp"
#include "sootc/Compiler/MethodCompiler.hpp"
#include "sootc/Compiler/StateCompiler.hpp"
#include "sootc/Compiler/TypeCompiler.hpp"
#include "sootc/Env/FileEnv.hpp"
#include "sootc/IR/IR_Node.hpp"
#include "sootc/IR/IR_Value.hpp"
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <utility>

using namespace carbon::files;
using namespace file_util;

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
    REGISTER_FORM("function", compile_lambda);
    REGISTER_FORM("begin",  compile_begin);
    REGISTER_FORM("defmethod", compile_defmethod);
    REGISTER_FORM("deftype", compile_deftype);
    REGISTER_FORM("defstate", compile_defstate);
    REGISTER_FORM("require", compile_require);
    REGISTER_FORM("define-extern", compile_define_extern);
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
    if (form.is_string()) { 
        // Создаем конкретный объект IR_Reg вместо абстрактного IR_Value
        // Type*, index (пока 0), is_arg (false)
        auto* target = new IR_Reg(ts_.lookup_type("string"), 0, false);
        
        std::string str_value = form.to_std_string(); 
        
        // Используем static_cast, так как IR_Reg наследуется от IR_Value
        env->emit(form, std::make_unique<IR_LoadString>(target, str_value));
        return target; 
    }
    if (form.is_pair()) {
        auto pair = form.as_pair();
        auto head = pair->car;

        if (head.is_symbol()) {
            std::string op = head.as_symbol().c_str();
            
            // 1. Пытаемся найти специальную форму (if, define, lambda, function...)
            auto it = m_forms.find(op);
            if (it != m_forms.end()) {
                return it->second(this, form, pair->cdr, env);
            }

            // 2. Если это не спец-форма, проверяем, определена ли такая функция/переменная
            // Это поможет отловить (funtion ...) до того, как оно станет CALL
            if (!env->lookup(op)) {
                lg::error("Compile error: unknown form or undefined symbol '{}'", op);
                // Вместо падения в CALL, лучше бросить исключение или вернуть IR_Error
                throw std::runtime_error(fmt::format("Undefined identifier: {}", op));
            }
        }

        // 3. Только если мы уверены, что голова списка — это что-то вызываемое
        lg::info("Compiling call to '{}'", head.is_symbol() ? head.as_symbol().c_str() : "lambda");
        return compile_call(form, env);
    }
    return nullptr;
}

// ============================================================================
// compile_module — один проход компиляции, потом сборка
// ============================================================================
std::shared_ptr<carbon::modules::Module> Compiler::compile_module(const script::Object& forms, FileEnv* env) {
    // ФАЗА 1: DECLARE (все формы, включая require)
    declare_module(forms, env);
    
    // ФАЗА 2: RESOLVE (компиляция тел функций и методов)
    resolve_module(env);
    
    // ФАЗА 3: BUILD (генерация бинарника)
    return build_module(env);
}

void Compiler::declare_module(const script::Object& forms, FileEnv* env) {
    auto current = forms;
    while (current.is_pair()) {
        compile(current.as_pair()->car, env);
        current = current.as_pair()->cdr;
    }
}

void Compiler::resolve_module(FileEnv* env) {
    // Рекурсивно резолвим все импортированные модули
    for (auto* imported : env->imports()) {
        resolve_module(imported);
    }
    
    // Резолвим символы текущего модуля
    for (auto& [name, value] : env->sybols_table()) {
        value->resolve(this);
    }
}

std::shared_ptr<carbon::modules::Module> Compiler::build_module(FileEnv* env) {
    static std::unordered_set<std::string> visited_modules;
    
    // Проверка на уже посещённые модули (избегаем циклов)
    if (visited_modules.find(env->name()) != visited_modules.end()) {
        return nullptr;  // Уже обработан
    }
    visited_modules.insert(env->name());
    
    // Рекурсивно билдим импортированные модули
    for (auto* imported : env->imports()) {
        auto imported_module = build_module(imported);
        if (imported_module) {
            // Сохраняем или линкуем импортированный модуль
            add_imported_module(imported_module);
        }
    }
    
    // Билдим текущий модуль
    RelocatableBuffer definitions("definitions", "definitions", true);
    RelocatableBuffer descriptors("descriptors", "descriptors", true);
    int definitions_count = 0;

    for (auto& [name, value] : env->symbols_map()) {  // Исправлено: symbols_map
        auto descriptor = value->build(this);
        if (!descriptor.is_empty()) {
            Definition definition{
                StringId(descriptor.name().c_str()),      // Добавлен .c_str()
                StringId(descriptor.type_tag().c_str()),  // Добавлен .c_str()
                SymbolFlags::Export,
                0,
                Ptr<u8>()  // nullptr
            };
            
            definitions.add_relocatable(offsetof(Definition, ptr), 
                                        Relocation::Type::LABEL_ADDRESS, 
                                        descriptor.name());
            definitions.add_label(name);                                        
            definitions.add_bytes(&definition, sizeof(Definition));
            descriptors.add_buffer(std::move(descriptor));  // Перемещаем
            definitions_count++;
        } 
    }
    
    // Создаем заголовок
    RelocatableBuffer file_buffer(env->name() + ".module", "module");
    
    // Заглушка для BinaryFile
    u32 header_pos = file_buffer.size();
    BinaryFile dummy_header{};
    file_buffer.add_bytes(&dummy_header, sizeof(BinaryFile));
    
    // Добавляем таблицу дефиниций
    u32 definitions_offset = file_buffer.size();
    file_buffer.add_buffer(std::move(definitions));  // Перемещаем
    
    // Добавляем дескрипторы
    u32 descriptors_offset = file_buffer.size();
    file_buffer.add_buffer(std::move(descriptors));  // Перемещаем
    
    // Патчим заголовок
    BinaryFile header{};
    header.base_offset = 0;
    header.magic = BinaryFile::MAGIC;
    header.generation = BinaryFile::CURRENT_GENERATION;
    header.definitions_count = definitions_count;
    header.definitions.offset = definitions_offset;
    header.file_size = file_buffer.size();
    header.used_size = file_buffer.size();
    
    file_buffer.write_at(header_pos, &header, sizeof(BinaryFile));
    
    // Линковка
    file_buffer.link_file();
    
    // Создаем модуль
    auto module = std::make_shared<Module>();
    module->name = StringId(env->name().c_str());
    
    // Копируем данные до уничтожения buffer
    auto file_data = std::move(file_buffer.bytes());
    module->set_file(std::move(file_data));
    
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

IR_Value* Compiler::compile_defstate(const script::Object& form, const script::Object& rest, Env* env) {
    StateCompiler state_compiler(ts_, this);
    return state_compiler.compile(form, rest, env);
}

// ============================================================================
// Мультимодульность
// ============================================================================

IR_Value* Compiler::compile_require(const script::Object& form, const script::Object& rest, Env* env) {
    auto args = rest;
    if (!args.is_pair() || !args.as_pair()->car.is_string()) {
        lg::error("Invalid require syntax: expected (require \"filename\")");
        return nullptr;
    }
    
    std::string filename = args.as_pair()->car.as_string()->data;
    
    // Проверяем, не загружен ли уже модуль
    if (m_compiled_modules.find(filename) != m_compiled_modules.end()) {
        lg::info("Module already loaded: {}", filename);
        return get_none();
    }
    
    
    FileEnv* current_env = dynamic_cast<FileEnv*>(env);
    if (!current_env) {
        throw std::runtime_error("require called outside of FileEnv");
    }
    
    // Проверяем, не импортирован ли уже
    if (current_env->has_import(filename)) {
        return get_none();
    }
    
    
    // Проверяем кэш компиляции
    auto it = m_compiled_files.find(filename);
    FileEnv* imported_env = nullptr;
    
    if (it != m_compiled_files.end()) {
        imported_env = it->second;
    } else {
        // Загружаем и компилируем только DECLARE фазу
        try {
            std::string source = read_text(filename);
            Reader reader;
            Object forms = reader.read_from_string(source, false, filename);
            
            imported_env = new FileEnv(env->global_env(), filename);
            
            // Только DECLARE фаза
            declare_module(forms, imported_env);
            
            m_compiled_files[filename] = imported_env;
            
        } catch (const std::exception& e) {
            throw std::runtime_error(fmt::format("Failed to require module '{}': {}", filename, e.what()));
        }
    }
    
    // Добавляем как зависимость
    current_env->add_import(imported_env);
    
    return get_none();
}

IR_Value* Compiler::compile_define_extern(const script::Object& form, const script::Object& rest, Env* env) {
    // (define-extern name type)
    // или (define-extern name type)
    auto args = rest;
    std::string name = args.as_pair()->car.as_symbol().c_str();
    auto type_form = args.as_pair()->cdr.as_pair()->car;
    
    // Парсим тип
    TypeSpec type_spec;
    if (type_form.is_symbol()) {
        type_spec = TypeSpec(type_form.as_symbol().c_str());
    } else if (type_form.is_pair()) {
        // Например, (function int int int)
        type_spec = parse_typespec(type_form, env);
    } else {
        lg::error("Invalid type specification in define-extern: {}", type_form.print());
        return nullptr;
    }
    
    // Создаем внешнее значение
    auto* extern_val = new IR_ExternValue(name, type_spec);
    
    // Если это тип, делаем forward declaration в TypeSystem
    if (type_spec.base_type() == "type") {
        // Это объявление типа (define-extern basic type)
        ts_.forward_declare_type_as(name, "object");  // или другой parent?
        // Создаем TypeEnv для forward-объявленного типа
        auto tyoe_type = ts_.lookup_type("type");
        auto* type_env = new TypeEnv(name, tyoe_type, env->global_env());
        env->bind(name, new IR_Type(type_env));
    } else {
        // Обычный внешний символ (функция, переменная)
        env->bind(name, extern_val);
    }
    
    return extern_val;
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

IR_Value* Compiler::compile_number(const script::Object& form, Env* env) {
    if (form.is_integer()) {
        s64 val = form.as_integer();
        
        // Анализ: если это целое, но оно используется там, где ожидается float 
        // (или если оно слишком большое для int32, если твоя ВМ 32-битная)
        // В данном случае просто создаем int-константу
        return IR_Const::create_int(ts_.lookup_type("int"), val);
    } 
    
    if (form.is_float()) {
        // Явное приведение к float убирает ошибку ambiguous call
        return IR_Const::create_float(ts_.lookup_type("float"), static_cast<float>(form.as_float()));
    }

    throw std::runtime_error("Unknown numeric form");
}

IR_Value* Compiler::compile_symbol(const script::Object& form, Env* env) {
    std::string name = form.as_symbol().c_str();
    
    // Keyword (начинается с ':') — самовычисляемая константа
    if (!name.empty() && name[0] == ':') {
        // Создаем константу типа keyword
        Type* keyword_type = ts_.lookup_type("symbol");
        auto string_id = static_cast<s64>(StringId(name));
        // TODO: нужно создать IR_Const, который хранит StringId
        return IR_Const::create_int(keyword_type,string_id); // placeholder
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

TypeSpec Compiler::parse_typespec(const script::Object& form, Env* env) {
    (void)env;  // может понадобиться для разрешения имен типов в будущем
    
    if (form.is_symbol()) {
        // Простое имя типа: "int", "float", "vector3"
        std::string name = form.as_symbol().c_str();
        return ts_.make_typespec(name);
    }
    
    if (form.is_pair()) {
        auto pair = form.as_pair();
        std::string type_name = pair->car.as_symbol().c_str();
        
        if (type_name == "_type_") {
            // В контексте метода _type_ означает тип this
            // Пока возвращаем как обычный тип
            return ts_.make_typespec("_type_");
        }

        if (type_name == "function") {
            // (function arg1 arg2 ... ret)
            std::vector<TypeSpec> args;
            auto current = pair->cdr;
            while (current.is_pair()) {
                args.push_back(parse_typespec(current.as_pair()->car, env));
                current = current.as_pair()->cdr;
            }
            if (args.empty()) {
                throw std::runtime_error("Function type must have at least return type");
            }
            // Последний аргумент — возвращаемый тип
            TypeSpec return_type = args.back();
            args.pop_back();
            
            TypeSpec result = ts_.make_typespec("function");
            for (const auto& arg : args) {
                result.add_arg(arg);
            }
            result.add_arg(return_type);
            return result;
        }
        
        if (type_name == "pointer") {
            // (pointer type)
            if (!pair->cdr.is_pair()) {
                throw std::runtime_error("Pointer type must have one argument");
            }
            TypeSpec pointee = parse_typespec(pair->cdr.as_pair()->car, env);
            return ts_.make_pointer_typespec(pointee);
        }
        
        if (type_name == "inline-array") {
            // (inline-array type)
            if (!pair->cdr.is_pair()) {
                throw std::runtime_error("Inline-array type must have one argument");
            }
            TypeSpec element = parse_typespec(pair->cdr.as_pair()->car, env);
            return ts_.make_inline_array_typespec(element);
        }
        
        if (type_name == "array") {
            // (array type)
            if (!pair->cdr.is_pair()) {
                throw std::runtime_error("Array type must have one argument");
            }
            TypeSpec element = parse_typespec(pair->cdr.as_pair()->car, env);
            return ts_.make_array_typespec("array", element);
        }
        
        // Составной тип с параметрами: (vector3 float float float)
        TypeSpec result = ts_.make_typespec(type_name);
        auto current = pair->cdr;
        while (current.is_pair()) {
            result.add_arg(parse_typespec(current.as_pair()->car, env));
            current = current.as_pair()->cdr;
        }
        return result;
    }
    
    throw std::runtime_error("Invalid type specification: " + form.print());
}

} // namespace sootc