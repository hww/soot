#include "common/soot/Printer.hpp"
#include "common/CommonTypes.hpp"
#include "common/soot/Object.hpp"
#include "common/util/PrintFloat.hpp"
#include <unordered_set>

#include <mutex>

#include "fmt/format.h"

namespace soot::pretty_print {

namespace {
// the integer representation is used here instead, wouldn't want really long numbers
const std::unordered_set<u32> banned_floats = {};

// print these floats (shown as ints here) as a named constant instead
const std::unordered_map<u32, std::string> const_floats = {{0x40490fda, "PI"},
                                                           {0xc0490fda, "MINUS_PI"}};
} // namespace

/*!
 * Print a float in a nice representation if possible, or an exact 32-bit integer constant to
 * be reinterpreted.
 */
soot::Object float_representation(float value) {
    u32 int_value;
    memcpy(&int_value, &value, 4);
    if (!proper_float(value)) {
        // lg::warn("PS2-incompatible float (0x{:08X}) detected! Writing as the-as cast.",
        // int_value);
        return pretty_print::build_list("the-as", "float",
                                        fmt::format("#x{:x}", (uint32_t)int_value));
    } else if (const_floats.find(int_value) != const_floats.end()) {
        return pretty_print::to_symbol(const_floats.at(int_value));
    } else if (banned_floats.find(int_value) == banned_floats.end()) {
        return soot::Object::make_float(value);
    } else {
        return pretty_print::build_list("the-as", "float", fmt::format("#x{:x}", int_value));
    }
}

std::mutex pretty_printer_reader_mutex;

soot::Object to_symbol(SymbolTable* st, const std::string &str) {
    std::lock_guard<std::mutex> guard(pretty_printer_reader_mutex);
    return Object::make_symbol(st, str.c_str());
}

soot::Object new_string(const std::string &str) {
    return Object::make_string(str);
}

soot::Object build_list(const std::string &str) {
    return build_list(to_symbol(str));
}

soot::Object build_list(const soot::Object &obj) {
    return Object::make_pair(obj, soot::Object::make_null());
}

soot::Object build_list(const std::vector<soot::Object> &objects) {
    if (objects.empty()) {
        return soot::Object::make_null();
    } else {
        return build_list(objects.data(), objects.size());
    }
}

// build a list out of an array of forms
soot::Object build_list(const soot::Object *objects, int count) {
    ASSERT(count);
    soot::Object result = soot::Object::make_null();
    for (int i = count; i-- > 0;) {
        result = Object::make_pair(objects[i], result);
    }

    return result;
}

// build a list out of a vector of strings that are converted to symbols
soot::Object build_list(const std::vector<std::string> &symbols) {
    if (symbols.empty()) {
        return soot::Object::make_null();
    }
    std::vector<soot::Object> f;
    f.reserve(symbols.size());
    for (auto &x : symbols) {
        f.push_back(to_symbol(x));
    }
    return build_list(f.data(), f.size());
}

void append(soot::Object &_in, const soot::Object &add) {
    auto *in = &_in;
    while (in->is_pair() && !in->as_pair()->cdr.is_null()) {
        in = &in->as_pair()->cdr;
    }

    if (!in->is_pair()) {
        ASSERT(false); // invalid list
    }
    in->as_pair()->cdr = add;
}

} // namespace soot::pretty_print
