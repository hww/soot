#include "common/sooti/Object.hpp"
#include "common/type_system/TypeSpec.hpp"
#include <string>
#include <unordered_map>

namespace script {
struct RegisterAlias : NativeRef {
    Object      name;
    Object      source;         // Ссылка родительский регистр
    Object      reg;            // Ссылка на физический регистр
    std::string type_name;      // Имя типа
    int         offset = 0;     // Смещение в БАЙТАХ
    int         bit_offset = 0; // Смещение в БИТАХ (0-7) внутри байта
    int         bit_size = 0;   // Размер поля в БИТАХ (если это битфилд)
    RegisterAlias() : name(Object::make_none()), reg(Object::make_none()), offset(0) {}
    Object      make_step_accessor(const Object &key) override;
    std::string print() const override {
        return fmt::format("#<reg-alias {} source:{} reg:{} type:{}>", name.print(), source.print(),
                           reg.print(), type_name);
    }
    Object inspect() const override;
};

class AsmRegsObject : public NativeRef {

  public:
    // 1. Явный публичный конструктор
    AsmRegsObject() : NativeRef() {}

    std::unordered_map<InternedSymbolPtr, std::shared_ptr<RegisterAlias>> aliases;

    // 2. Реализация обязательных методов HeapObject
    std::string class_name() const override {
        return "AsmRegsObject";
    }
    std::string type_name() const override {
        return "asm-regs";
    }

    // Если в HeapObject эти методы были = 0, их НУЖНО реализовать:
    Object inspect() const override {
        return Object::make_pair(Object::make_symbol(type_name()), Object::make_null());
    }
    std::string print() const override {
        return "#<asm-context with " + std::to_string(aliases.size()) + " aliases>";
    }

    // Реализуем accessor, если он нужен для (obj.field)
    Object make_step_accessor(const Object &key) override;

    bool is_table() const override {
        return true;
    }

    Object get_at(const Object &key) override {
        if (key.type != ObjectType::SYMBOL)
            return Object::make_none();
        auto it = aliases.find(key.symbol_obj.value);
        if (it != aliases.end()) {
            return Object::make_native_ref(it->second);
        }
        return Object::make_none();
    }

    // Добавим пустой set_at, если он требуется интерфейсом
    void set_at(const Object &key, const Object &val) override {
        // Здесь можно реализовать динамическое добавление из Лиспового кода
        (void)key;
        (void)val;
    }

    void set_at(const InternedSymbolPtr &key, std::shared_ptr<RegisterAlias> &val) {
        aliases[key] = val;
    }
};

class AsmEnvironmentObject : public EnvironmentObject {
  public:
    // Тот самый объект с регистрами, который мы уже написали
    std::shared_ptr<AsmRegsObject> asm_regs;

    AsmEnvironmentObject(std::shared_ptr<EnvironmentObject> parent = nullptr)
        : EnvironmentObject(std::move(parent)) {
        asm_regs = std::make_shared<AsmRegsObject>();
    }

    // Переопределяем поиск метаданных или специфический поиск
    // чтобы макросы могли найти "своих"
    Object get_asm_context() {
        return Object::make_native_ref(asm_regs);
    }

    std::string class_name() const override {
        return "AsmEnvironmentObject";
    }
};

Object get_current_asm_context(std::shared_ptr<EnvironmentObject> env);
} // namespace script