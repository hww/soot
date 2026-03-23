#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sootc/IR/IR_Node.hpp"
#include "common/sootc/IR/IR_Value.hpp"
#include <memory>
#include <vector>

namespace sootc {

class FunctionCompiler {
  public:
    FunctionCompiler(TypeSystem &ts, Type *function_type, const std::string& func_name);

    // Создание регистров
    IR_Reg *create_local_reg(Type *type);
    IR_Reg* create_arg_reg(Type* type, u32 index);
    IR_Reg *get_self_reg();

    // Добавление узлов
    void add_node(std::unique_ptr<IR_Node> node);

    // Генерация байткода
    std::vector<u8> compile();

    // Отладка
    std::string to_string() const;

  private:
    TypeSystem &ts_;
    Type       *function_type_;
    std::string function_name_;

    std::vector<std::unique_ptr<IR_Value>> values_;
    std::vector<std::unique_ptr<IR_Reg>>   regs_;
    std::vector<std::unique_ptr<IR_Node>>  nodes_;
    u32                                    next_reg_ = 0;
};

} // namespace sootc
