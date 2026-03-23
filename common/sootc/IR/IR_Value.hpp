#pragma once

#include "common/type_system/Type.hpp"
#include <string>

namespace sootc {

static constexpr u32 ARG_REGISTERS_OFFSET = 24;

class IR_Value {
  public:
    explicit IR_Value(Type *type) : type_(type) {}
    virtual ~IR_Value() = default;
    virtual std::string to_string() const = 0;
    virtual bool        is_reg() const {
        return false;
    }
    virtual bool is_const() const {
        return false;
    }
    virtual bool is_field() const {
        return false;
    }
    Type *get_type() const {
        return type_;
    }

  protected:
    Type *type_;
};

class IR_Reg : public IR_Value {
  public:
    IR_Reg(Type *type, u32 index, bool is_arg = false);
    std::string to_string() const override;
    bool        is_reg() const override {
        return true;
    }
    u32 get_index() const {
        return index_;
    }
    bool is_argument() const {
        return is_arg_;
    }
    static constexpr u32 REG_SELF = 24;
    static constexpr u32 REG_RETURN = 0;

  private:
    u32  index_;
    bool is_arg_;
};

class IR_Const : public IR_Value {
  public:
    IR_Const(Type *type, s64 val);
    IR_Const(Type *type, float val);
    std::string to_string() const override;
    bool        is_const() const override {
        return true;
    }
    bool is_float() const {
        return is_float_;
    }
    s64 get_int() const {
        return int_val_;
    }
    float get_float() const {
        return float_val_;
    }

  private:
    s64   int_val_ = 0;
    float float_val_ = 0.0f;
    bool  is_float_ = false;
};

class IR_Field : public IR_Value {
  public:
    IR_Field(IR_Value *base, const Field &field);
    std::string to_string() const override;
    bool        is_field() const override {
        return true;
    }
    IR_Value *get_base() const {
        return base_;
    }
    const Field &get_field() const {
        return field_;
    }
    int get_offset() const {
        return field_.offset();
    }

  private:
    IR_Value *base_;
    Field     field_;
};

} // namespace sootc
