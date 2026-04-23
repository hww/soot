#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/soot/Object.hpp"

namespace soot {
    bool get_va(const Object& rest, std::string* err_string, Arguments* result);
    void get_va_no_named(const Object& rest, Arguments* result);
    bool va_check(
        const Arguments& args,
        const std::vector<std::optional<ObjectType>>& unnamed,
        const std::unordered_map<std::string, std::pair<bool, std::optional<ObjectType>>>& named,
        std::string* err_string);

    template <typename T>
    void for_each_in_list(const Object& list, T f) {
        const Object* iter = &list;
        while (iter->is_pair()) {
            const auto& lap = iter->as_pair();
            f(lap->car);
            iter = &lap->cdr;
        }

        ASSERT(iter->is_null());
    }

    int list_length(const Object& list);
}  // namespace goos
