
#include "common/sooti/Interpreter.hpp"
#include "common/sooti/PrettyPrinter.hpp"
#include "common/sooti/Errors.hpp"
#include "common/sooti/Printer.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/Reader.hpp"

#include "fmt/args.h"
#include "fmt/base.h"
#include "fmt/format.h"
#include "fmt/color.h"
#include "common/util/Log.hpp"
#include "common/util/Crc32.hpp"
#include "common/util/FileUtil.hpp"
#include "common/util/StringUtil.hpp"
#include "common/util/UnicodeUtil.hpp"
#include "common/util/StringUtil.hpp"

#include "common/CommonTypes.hpp"
#include "common/versions/version.h"
#include "common/versions/revision.h"
#include <sstream>
#include <filesystem>
#include <set>
#include "common/type_system/TypeSystem.hpp"
#include "common/type_system/Defenum.hpp"
#include "common/type_system/Deftype.hpp"
#include "common/type_system/TypeSpec.hpp"

namespace script
{

    SootTypeSystem::SootTypeSystem(Interpreter &interpreter) : m_type_system(std::make_unique<TypeSystem>()), m_interpreter(interpreter) {}
    SootTypeSystem::~SootTypeSystem() = default;

    void SootTypeSystem::init_type_system(BaseTyles types)
    {
        m_type_system.get()->clear();
        switch (types) 
        {
            case BaseTyles::Z80:
                m_type_system.get()->add_builtin_types_z80();
                break;
            default:
                m_type_system.get()->add_builtin_types();
                break;
        }
    }

    Object SootTypeSystem::eval_defenum_special(const Object &form, const Object &rest, const std::shared_ptr<EnvironmentObject> &env)
    {
        DefinitionMetadata meta{};
        EnumType *type = parse_defenum(rest, m_type_system.get(), &meta);
        return m_interpreter.get_nil();
    }
    Object SootTypeSystem::eval_deftype_special(const Object &form, const Object &rest, const std::shared_ptr<EnvironmentObject> &env)
    {
        (void)form;
        auto map = m_interpreter.get_global_environment().as_env();
        // Используем rest 
        // Не form (deftype name ...)
        // А rest (name ...) 
        DeftypeResult result = parse_deftype(rest, m_type_system.get(), &map->vars);
        return m_interpreter.get_nil();
    }
    Object SootTypeSystem::eval_typespec_special(const Object &form, const Object &rest, const std::shared_ptr<EnvironmentObject> &env)
    {      
        (void)form;
        // rest — это то, что передали в (type-spec ...)
        // Например, если вызвали (type-spec (pointer int32)), то rest это ((pointer int32))
        
        // В GOAL обычно передают один аргумент в typespec
        Object spec_input = rest.as_pair()->car; 
        
        TypeSpec ts = parse_typespec(m_type_system.get(), spec_input);
        
        // Возвращаем как S-выражение
        return type_spec_to_lisp(ts);
    }

    // --- Printing ----------------------------------------------------------------------
    Object SootTypeSystem::eval_type_to_lisp(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env){
        (void)env;
        m_interpreter.vararg_check(form, args, { ObjectType::PAIR }, {}); // Пара и значение
        TypeSpec ts = parse_typespec(m_type_system.get(), args.unnamed[0]);
        Type* t = m_type_system->lookup_type(ts);
        if (t == nullptr) 
            return m_interpreter.get_nil();
        return type_to_lisp(t);
    }
    Object SootTypeSystem::eval_types_list(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env)
    {
        return m_type_system.get()->get_all_type_names_as_objects();
    }
    Object SootTypeSystem::eval_init_types(const Object& form, Arguments& args, const std::shared_ptr<EnvironmentObject>& env)
    {
        m_interpreter.vararg_check(form, args, { ObjectType::SYMBOL }, {}); 
        if (args.unnamed[0].as_symbol() == "default")
            init_type_system(BaseTyles::Default);
        else if (args.unnamed[0].as_symbol() == "z80")
            init_type_system(BaseTyles::Z80);
        else
             m_interpreter.throw_eval_error(form, "init-types: arg 1 expects symbol 'default or 'z80");
        return m_interpreter.get_nil();
    }
    // --- Helpers ----------------------------------------------------------------------
    // В файле реализации SootTypeSystem.cpp:
    Object SootTypeSystem::type_to_lisp(const Type* type) const {
        if (!type) {
            return Object::make_empty_list();
        }

        // Создаем ассоциативный список вида ((key . value) ...)
        std::vector<Object> assoc_pairs;

        // 1. Базовая информация о типе
        assoc_pairs.push_back(Object::make_pair(
            Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "name"),
            Object::make_string(type->get_name())
        ));

        assoc_pairs.push_back(Object::make_pair(
            Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "runtime-name"),
            Object::make_string(type->get_runtime_name())
        ));

        assoc_pairs.push_back(Object::make_pair(
            Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "parent"),
            Object::make_string(type->get_parent())
        ));

        assoc_pairs.push_back(Object::make_pair(
            Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "boxed"),
            type->is_boxed() ? m_interpreter.m_object_true : m_interpreter.m_object_false
        ));

        assoc_pairs.push_back(Object::make_pair(
            Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "heap-base"),
            Object::make_integer(type->heap_base())
        ));

        // 2. Тип-специфичная информация
        if (auto* value_type = dynamic_cast<const ValueType*>(type)) {
            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "type-category"),
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "value-type")
            ));

            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "size"),
                Object::make_integer(value_type->get_size_in_memory())
            ));

            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "load-size"),
                Object::make_integer(value_type->get_load_size())
            ));

            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "sign-extend"),
                value_type->get_load_signed() ? m_interpreter.m_object_true : m_interpreter.m_object_false
            ));

            // Для перечислений
            if (auto* enum_type = dynamic_cast<const EnumType*>(type)) {
                Object entries_list = Object::make_empty_list();
                
                // Собираем список в обратном порядке
                for (const auto& [name, value] : enum_type->entries()) {
                    Object entry = Object::make_pair(
                        Object::make_string(name),
                        Object::make_integer(value)
                    );
                    entries_list = Object::make_pair(entry, entries_list);
                }
                
                assoc_pairs.push_back(Object::make_pair(
                    Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "enum-entries"),
                    entries_list
                ));

                assoc_pairs.push_back(Object::make_pair(
                    Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "bitfield"),
                    enum_type->is_bitfield() ? m_interpreter.m_object_true : m_interpreter.m_object_false
                ));
            }
            
            // Для битовых полей
            else if (auto* bitfield_type = dynamic_cast<const BitFieldType*>(type)) {
                Object fields_list = Object::make_empty_list();
                
                for (const auto& field : bitfield_type->fields()) {
                    Object field_info = build_list({
                        Object::make_string(field.name()),
                        Object::make_integer(field.offset()),
                        Object::make_integer(field.size()),
                        type_spec_to_lisp(field.type())
                    });
                    
                    fields_list = Object::make_pair(field_info, fields_list);
                }
                
                assoc_pairs.push_back(Object::make_pair(
                    Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "bitfields"),
                    fields_list
                ));
            }
        }
        else if (auto* struct_type = dynamic_cast<const StructureType*>(type)) {
            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "type-category"),
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "structure-type")
            ));

            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "size"),
                Object::make_integer(struct_type->get_size_in_memory())
            ));

            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "dynamic"),
                struct_type->is_dynamic() ? m_interpreter.m_object_true : m_interpreter.m_object_false
            ));

            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "packed"),
                struct_type->is_packed() ? m_interpreter.m_object_true : m_interpreter.m_object_false
            ));

            // Поля структуры
            Object fields_list = Object::make_empty_list();
            
            for (const auto& field : struct_type->fields()) {
                Object field_info = build_list({
                    Object::make_string(field.name()),
                    Object::make_integer(field.offset()),
                    type_spec_to_lisp(field.type()),
                    field.is_inline() ? m_interpreter.m_object_true : m_interpreter.m_object_false,
                    field.is_array() ? m_interpreter.m_object_true : m_interpreter.m_object_false,
                    field.is_dynamic() ? m_interpreter.m_object_true : m_interpreter.m_object_false,
                    Object::make_integer(field.array_size())
                });
                
                fields_list = Object::make_pair(field_info, fields_list);
            }
            
            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "fields"),
                fields_list
            ));
            
            // Для BasicType добавляем специальный признак
            if (dynamic_cast<const BasicType*>(type)) {
                assoc_pairs.push_back(Object::make_pair(
                    Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "basic-type"),
                    m_interpreter.m_object_true
                ));
            }
        }
        else if (auto* ref_type = dynamic_cast<const ReferenceType*>(type)) {
            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "type-category"),
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "reference-type")
            ));
        }
        else if (dynamic_cast<const NullType*>(type)) {
            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "type-category"),
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "null-type")
            ));
        }

        // 3. Методы
        Object methods_list = Object::make_empty_list();
        const auto& methods = type->get_methods_defined_for_type();
        
        for (const auto& method : methods) {
            Object method_info = build_list({
                Object::make_string(method.name),
                Object::make_integer(method.id),
                type_spec_to_lisp(method.type),
                Object::make_string(method.defined_in_type)
            });
            
            methods_list = Object::make_pair(method_info, methods_list);
        }
        
        assoc_pairs.push_back(Object::make_pair(
            Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "methods"),
            methods_list
        ));

        // 4. Состояния (states)
        Object states_list = Object::make_empty_list();
        const auto& states = type->get_states_declared_for_type();
        
        for (const auto& [name, ts] : states) {
            Object state_info = Object::make_pair(
                Object::make_string(name),
                type_spec_to_lisp(ts)
            );
            
            states_list = Object::make_pair(state_info, states_list);
        }
        
        assoc_pairs.push_back(Object::make_pair(
            Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "states"),
            states_list
        ));

        // 5. Метод new (если есть)
        MethodInfo new_method;
        if (type->get_my_new_method(&new_method)) {
            Object new_method_info = build_list({
                Object::make_string(new_method.name),
                Object::make_integer(new_method.id),
                type_spec_to_lisp(new_method.type)
            });
            
            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "new-method"),
                new_method_info
            ));
        }

        // 6. Metadata
        if (type->m_metadata.has_location()) {
            const auto& info = type->m_metadata.definition_info.value();
            Object location_info = build_list({
                Object::make_string(info.filename),
                Object::make_integer(info.line_idx_to_display),
                Object::make_integer(info.pos_in_line)
            });
            
            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "defined-at"),
                location_info
            ));
        }
        
        if (type->m_metadata.has_docstring()) {
            assoc_pairs.push_back(Object::make_pair(
                Object::make_symbol(&m_interpreter.m_reader.get_symbol_table(), "docstring"),
                Object::make_string(type->m_metadata.get_docstring_or_empty())
            ));
        }

        // Собираем все пары в один ассоциативный список
        Object result = Object::make_empty_list();
        for (auto it = assoc_pairs.rbegin(); it != assoc_pairs.rend(); ++it) {
            result = Object::make_pair(*it, result);
        }

        return result;
    }

    // Вспомогательная функция для преобразования TypeSpec
    Object SootTypeSystem::type_spec_to_lisp(const TypeSpec& ts) const {
        if (ts.base_type() == "none" || ts.base_type().empty()) {
            return m_interpreter.get_nil();
        }

        Object base = m_interpreter.intern(ts.base_type());

        // Если нет ни аргументов, ни тегов — возвращаем просто символ (атом)
        if (ts.get_args_count() == 0 && ts.get_tags_count() == 0) {
            return base;
        }

        // Если есть хоть что-то — строим список (base args... tags...)
        std::vector<Object> list_elements;
        list_elements.push_back(base);

        // 1. Добавляем вложенные TypeSpec (аргументы)
        for (size_t i = 0; i < ts.get_args_count(); ++i) {
            list_elements.push_back(type_spec_to_lisp(ts.get_arg(i)));
        }

        // 2. Добавляем теги в формате :key value
        for (const auto& tag : ts.get_tags()) {
            // Превращаем имя тега в ключевое слово (например, "foo" -> ":foo")
            std::string keyword = ":" + tag.name;
            list_elements.push_back(m_interpreter.intern(keyword));
            
            // Значение тега (предполагаем, что это может быть имя типа или строка)
            // Если значение выглядит как число, можно было бы парсить, 
            // но пока оставим как символ/строку для простоты
            list_elements.push_back(m_interpreter.intern(tag.value));
        }

        return pretty_print::build_list(list_elements);
    }

    // Вспомогательная функция для построения списка
    Object SootTypeSystem::build_list(const std::vector<Object>& objects) const {
        Object result = Object::make_empty_list();
        for (auto it = objects.rbegin(); it != objects.rend(); ++it) {
            result = Object::make_pair(*it, result);
        }
        return result;
    }
}
