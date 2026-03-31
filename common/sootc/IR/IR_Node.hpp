#pragma once

#include "common/carbon/vm/Instructions.hpp"
#include "common/sootc/IR/IR_Value.hpp"
#include <unordered_map>
#include <vector>

using namespace carbon::vm;

namespace sootc {

// Forward declaration
class FunctionDescBuilder;

// Базовый класс IR узла
class IR_Node {
public:
  virtual ~IR_Node() = default;
  virtual std::string to_string() const = 0;
  virtual void generate(FunctionDescBuilder &builder,
                        const std::unordered_map<IR_Value *, u32> &reg_map) = 0;
  virtual std::vector<IR_Value *> get_used_values() const { return {}; }
};

// Перемещение
class IR_Move : public IR_Node {
public:
  IR_Move(IR_Reg *dest, IR_Value *src);
  std::string to_string() const override;
  void generate(FunctionDescBuilder &builder,
                const std::unordered_map<IR_Value *, u32> &reg_map) override;
  std::vector<IR_Value *> get_used_values() const override;

private:
  IR_Reg *dest_;
  IR_Value *src_;
};

// Загрузка константы
class IR_LoadConst : public IR_Node {
public:
  IR_LoadConst(IR_Reg *dest, IR_Const *value);
  std::string to_string() const override;
  void generate(FunctionDescBuilder &builder,
                const std::unordered_map<IR_Value *, u32> &reg_map) override;
  std::vector<IR_Value *> get_used_values() const override;
private:
  IR_Reg *dest_;
  IR_Const *value_;
};

// Загрузка поля
class IR_LoadField : public IR_Node {
public:
  IR_LoadField(IR_Reg *dest, IR_Field *field);
  std::string to_string() const override;
  void generate(FunctionDescBuilder &builder,
                const std::unordered_map<IR_Value *, u32> &reg_map) override;
  std::vector<IR_Value *> get_used_values() const override;
private:
  IR_Reg *dest_;
  IR_Field *field_;
};

// Сохранение в поле
class IR_StoreField : public IR_Node {
public:
  IR_StoreField(IR_Field *field, IR_Value *value);
  std::string to_string() const override;
  void generate(FunctionDescBuilder &builder,
                const std::unordered_map<IR_Value *, u32> &reg_map) override;
  std::vector<IR_Value *> get_used_values() const override;
private:
  IR_Field *field_;
  IR_Value *value_;
};

// Вызов функции/метода
class IR_Call : public IR_Node {
public:
  IR_Call(IR_Reg *result, IR_Value *function, IR_Value *this_ptr,
          std::vector<IR_Value *> args);
  std::string to_string() const override;
  void generate(FunctionDescBuilder &builder,
                const std::unordered_map<IR_Value *, u32> &reg_map) override;
  std::vector<IR_Value *> get_used_values() const override;
private:
  IR_Reg *result_;
  IR_Value *function_;
  IR_Value *this_ptr_;
  std::vector<IR_Value *> args_;
};

// Арифметическая операция
class IR_Binary : public IR_Node {
public:
  enum class Op { ADD, SUB, MUL, DIV, MOD, AND, OR, XOR };
  IR_Binary(Op op, IR_Reg *dest, IR_Value *left, IR_Value *right);
  std::string to_string() const override;
  void generate(FunctionDescBuilder &builder,
                const std::unordered_map<IR_Value *, u32> &reg_map) override;
  std::vector<IR_Value*> get_used_values() const override {
      // ОБЯЗАТЕЛЬНО возвращаем и цель, и операнды
      return { dest_, left_, right_ }; 
  }
private:
  Op op_;
  IR_Reg *dest_;
  IR_Value *left_;
  IR_Value *right_;
};

// Сравнение
class IR_Compare : public IR_Node {
public:
  enum class Cond { EQ, NE, LT, LE, GT, GE };
  IR_Compare(Cond cond, IR_Reg *dest, IR_Value *left, IR_Value *right);
  std::string to_string() const override;
  void generate(FunctionDescBuilder &builder,
                const std::unordered_map<IR_Value *, u32> &reg_map) override;
  std::vector<IR_Value*> get_used_values() const override ;
private:
  Cond cond_;
  IR_Reg *dest_;
  IR_Value *left_;
  IR_Value *right_;
};

// Условный переход
class IR_BranchIf : public IR_Node {
public:
  IR_BranchIf(IR_Value *cond, int true_label, int false_label = -1);
  std::string to_string() const override;
  void generate(FunctionDescBuilder &builder,
                const std::unordered_map<IR_Value *, u32> &reg_map) override;

private:
  IR_Value *cond_;
  int true_label_;
  int false_label_;
};

// Безусловный переход
class IR_Branch : public IR_Node {
public:
  explicit IR_Branch(int label);
  std::string to_string() const override;
  void generate(FunctionDescBuilder &builder,
                const std::unordered_map<IR_Value *, u32> &reg_map) override;

private:
  int label_;
};

// Метка
class IR_Label : public IR_Node {
public:
  explicit IR_Label(int label);
  std::string to_string() const override;
  void generate(FunctionDescBuilder &builder,
                const std::unordered_map<IR_Value *, u32> &reg_map) override;

private:
  int label_;
};

// Возврат
class IR_Return : public IR_Node {
public:
  explicit IR_Return(IR_Value *value = nullptr);
  std::string to_string() const override;
  void generate(FunctionDescBuilder &builder,
                const std::unordered_map<IR_Value *, u32> &reg_map) override;
  std::vector<IR_Value *> get_used_values() const override;
private:
  IR_Value *value_;
};

// Вспомогательный класс для сборки байткода
class FunctionDescBuilder {
public:
  void add_instruction(Opcode op, u8 a = 0, u8 b = 0, u16 c = 0) {
    instructions_.push_back(
        Instruction::create_abc(op, a, b, static_cast<u8>(c)));
  }

  void add_label(int label) { labels_[label] = instructions_.size(); }

  void add_branch_label(int label) {
    branch_labels_.push_back({label, instructions_.size()});
    add_instruction(Opcode::NOOP); // placeholder
  }

  void resolve_labels() {
    for (auto &[label, pos] : branch_labels_) {
      u32 target = labels_[label];
      instructions_[pos].k = static_cast<u16>(target);
    }
  }

  std::vector<Instruction> get_instructions() {
    resolve_labels();
    return instructions_;
  }

private:
  std::vector<Instruction> instructions_;
  std::unordered_map<int, u32> labels_;
  std::vector<std::pair<int, u32>> branch_labels_;
};
} // namespace sootc
