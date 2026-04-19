// IR_Expression.hpp
#pragma once

#include "common/sootc/IR/IR_Value.hpp"
#include "common/sootc/IR/IR_Value.hpp"
#include "sootc/Env/Label.hpp"
#include <string>
#include <vector>

using namespace carbon;

namespace sootc {

// Forward declaration
struct Label;

// Базовый класс для всех выражений - теперь использует правильную иерархию
// IR_Expression наследует IR_Value и добавляет generate()
class IR_Expression : public IR_Value {
public:
    using IR_Value::IR_Value; // Пробрасываем конструктор
    virtual ~IR_Expression() = default;
};

// Перемещение
class IR_Move : public IR_Expression {
public:
    IR_Move(IR_Reg *dest, IR_Value *src);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    std::vector<IR_Value *> get_used_values() const override;
    void resolve(Compiler* c) override;

private:
    IR_Reg *dest_;
    IR_Value *src_;
};

// Загрузка константы
class IR_LoadConst : public IR_Expression {
public:
    IR_LoadConst(IR_Reg *dest, IR_Const *value);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    std::vector<IR_Value *> get_used_values() const override;
    void resolve(Compiler* c) override;

private:
    IR_Reg *dest_;
    IR_Const *value_;
};

// Загрузка строки
class IR_LoadString : public IR_Expression {
public:
    IR_LoadString(IR_Reg* dest, const std::string& value);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    std::vector<IR_Value*> get_used_values() const override;
    void resolve(Compiler* c) override;

private:
    IR_Reg* dest_;
    std::string value_;
};

// Загрузка поля
class IR_LoadField : public IR_Expression {
public:
    IR_LoadField(IR_Reg *dest, IR_Value *base, u32 offset, Type* field_type);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    std::vector<IR_Value *> get_used_values() const override;
    void resolve(Compiler* c) override;

private:
    IR_Reg *dest_;
    IR_Value *base_;
    u32 offset_;
    Type* field_type_;
};

// Сохранение в поле
class IR_StoreField : public IR_Expression {
public:
    IR_StoreField(IR_Value *base, u32 offset, IR_Value *value);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    std::vector<IR_Value *> get_used_values() const override;
    void resolve(Compiler* c) override;

private:
    IR_Value *base_;
    u32 offset_;
    IR_Value *value_;
};

// Вызов функции/метода
class IR_Call : public IR_Expression {
public:
    IR_Call(IR_Reg *result, IR_Value *function, IR_Value *this_ptr, std::vector<IR_Value *> args);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    std::vector<IR_Value *> get_used_values() const override;
    void resolve(Compiler* c) override;

private:
    IR_Reg *result_;
    IR_Value *function_;
    IR_Value *this_ptr_;
    std::vector<IR_Value *> args_;
};

// Арифметическая операция
class IR_Binary : public IR_Expression {
public:
    enum class Op { ADD, SUB, MUL, DIV, MOD, AND, OR, XOR };
    IR_Binary(Op op, IR_Reg *dest, IR_Value *left, IR_Value *right);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    std::vector<IR_Value*> get_used_values() const override;
    void resolve(Compiler* c) override;

private:
    Op op_;
    IR_Reg *dest_;
    IR_Value *left_;
    IR_Value *right_;
};

// Сравнение
class IR_Compare : public IR_Expression {
public:
    enum class Cond { EQ, NE, LT, LE, GT, GE };
    IR_Compare(Cond cond, IR_Reg *dest, IR_Value *left, IR_Value *right);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    std::vector<IR_Value*> get_used_values() const override;
    void resolve(Compiler* c) override;

private:
    Cond cond_;
    IR_Reg *dest_;
    IR_Value *left_;
    IR_Value *right_;
};

// Условный переход
class IR_BranchIf : public IR_Expression {
public:
    IR_BranchIf(IR_Value *cond, Label true_label);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    void resolve(Compiler* c) override;

private:
    IR_Value *cond_;
    Label true_label_;
};

// Безусловный переход
class IR_Branch : public IR_Expression {
public:
    explicit IR_Branch(Label label);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    void resolve(Compiler* c) override;

private:
    Label label_;
};

// Метка
class IR_Label : public IR_Expression {
public:
    explicit IR_Label(Label label);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    void resolve(Compiler* c) override;

private:
    Label label_;
};

// Возврат
class IR_Return : public IR_Expression {
public:
    explicit IR_Return(IR_Value *value = nullptr);
    std::string to_string() const override;
    void emit(Env& env, Compiler* compiler) override;
    std::vector<IR_Value *> get_used_values() const override;
    void resolve(Compiler* c) override;

private:
    IR_Value *value_;
};

// Доступ к полю (как выражение)
class IR_FieldAccess : public IR_Expression {
public:
    IR_FieldAccess(IR_Value *base, const Field &field);
    std::string to_string() const override;
    bool is_field() const override { return true; }
    void emit(Env& env, Compiler* compiler) override;
    std::vector<IR_Value*> get_used_values() const override;
    void resolve(Compiler* c) override;

    IR_Value *get_base() const { return base_; }
    const Field &get_field() const { return field_; }
    int get_offset() const { return field_.offset(); }

private:
    IR_Value *base_;
    Field field_;
};

} // namespace sootc