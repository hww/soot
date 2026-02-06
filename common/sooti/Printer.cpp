#include "common/sooti/Printer.hpp"
#include "common/CommonTypes.hpp"
#include "common/sooti/Object.hpp"
#include "common/util/PrintFloat.hpp"
#include <unordered_set>

#include <cmath>
#include <mutex>

#include "fmt/format.h"

namespace script::pretty_print {

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
script::Object float_representation(float value) {
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
        return script::Object::make_float(value);
    } else {
        return pretty_print::build_list("the-as", "float", fmt::format("#x{:x}", int_value));
    }
}

std::mutex pretty_printer_reader_mutex;

script::Object to_symbol(const std::string &str) {
    std::lock_guard<std::mutex> guard(pretty_printer_reader_mutex);
    return Object::make_symbol(str.c_str());
}

script::Object new_string(const std::string &str) {
    return Object::make_string(str);
}

script::Object build_list(const std::string &str) {
    return build_list(to_symbol(str));
}

script::Object build_list(const script::Object &obj) {
    return Object::make_pair(obj, script::Object::make_null());
}

script::Object build_list(const std::vector<script::Object> &objects) {
    if (objects.empty()) {
        return script::Object::make_null();
    } else {
        return build_list(objects.data(), objects.size());
    }
}

// build a list out of an array of forms
script::Object build_list(const script::Object *objects, int count) {
    ASSERT(count);
    script::Object result = script::Object::make_null();
    for (int i = count; i-- > 0;) {
        result = Object::make_pair(objects[i], result);
    }

    return result;
}

// build a list out of a vector of strings that are converted to symbols
script::Object build_list(const std::vector<std::string> &symbols) {
    if (symbols.empty()) {
        return script::Object::make_null();
    }
    std::vector<script::Object> f;
    f.reserve(symbols.size());
    for (auto &x : symbols) {
        f.push_back(to_symbol(x));
    }
    return build_list(f.data(), f.size());
}

void append(script::Object &_in, const script::Object &add) {
    auto *in = &_in;
    while (in->is_pair() && !in->as_pair()->cdr.is_null()) {
        in = &in->as_pair()->cdr;
    }

    if (!in->is_pair()) {
        ASSERT(false); // invalid list
    }
    in->as_pair()->cdr = add;
}

} // namespace script::pretty_print
