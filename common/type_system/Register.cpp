#include "Register.hpp"
#include "script/ListBuilder.hpp"
#include "TypeSystem.hpp"
#include "script/Object.hpp"

namespace script {

Object Register::inspect(SymbolTable* st) const {
    ListBuilder lb(st);
    lb.add_symbol("reg-alias");
    lb.add_key_value("name", name);
    lb.add_key_value("physical-reg", reg);
    lb.add_key_value("type-name", type_name);
    lb.add_key_value("offset", Object::make_integer(offset));
    lb.add_key_value("bit-offset", Object::make_integer(bit_offset));
    lb.add_key_value("bit-ize", Object::make_integer(bit_size));
    return lb.build();
}

std::string Register::print() const {
    return fmt::format("#<reg-alias {} :type {} :reg {} :offset {} :bit-offset {} :bit-size {}>",
                       name.print(), type_name.print(), reg.print(), offset, bit_offset, bit_size);
}
} // namespace soot
