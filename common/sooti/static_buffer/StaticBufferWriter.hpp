#pragma once

#include "common/sooti/Object.hpp"

class Type;
class EnumType;
class ValueType;
class BitFieldType;

namespace script {

class StaticBufferWriter {
  public:
    /**
     * Главная точка входа для записи значения по указателю.
     * Используется TypePointer::set().
     */
    static void write_value_at_ptr(void *ptr, Type *type, const Object &val);

  private:
    static void write_primitive_at_ptr(void *ptr, ValueType *type, const Object &val);
    static void write_enum_at_ptr(void *ptr, EnumType *type, const Object &val);
    static void write_bitfield_at_ptr(void *ptr, BitFieldType *type, const Object &val);
    static void write_string_at_ptr(void *ptr, Type *type, const Object &val);
};

} // namespace script