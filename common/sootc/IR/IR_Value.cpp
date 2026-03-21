// common/sootc/src/IR/IR_Value.cpp
#include "common/sootc/IR/IR_Value.hpp"

namespace sootc {

IR_Reg::IR_Reg(Type *type, u32 index, bool is_arg)
    : IR_Value(type), index_(index), is_arg_(is_arg) {}

std::string IR_Reg::to_string() const {
    if (is_arg_) {
        return "arg" + std::to_string(index_);
    }
    return "r" + std::to_string(index_);
}

IR_Const::IR_Const(Type *type, s64 val) : IR_Value(type), int_val_(val), is_float_(false) {}

IR_Const::IR_Const(Type *type, float val) : IR_Value(type), float_val_(val), is_float_(true) {}

std::string IR_Const::to_string() const {
    if (is_float_) {
        return std::to_string(float_val_);
    }
    return std::to_string(int_val_);
}

IR_Field::IR_Field(IR_Value *base, const Field &field)
    : IR_Value(field.type().get()), base_(base), field_(field) {}

std::string IR_Field::to_string() const {
    return base_->to_string() + "." + field_.name();
}

} // namespace sootc