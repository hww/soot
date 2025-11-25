#pragma once

#include <string>
#include <vector>

#include "common/script/object.h"
#include "common/script/reader.h"

namespace script::pretty_print {
	// string -> object (as a symbol)
	Object to_symbol(const std::string& str);

	Object new_string(const std::string& str);

	// list with a single symbol from a string
	Object build_list(const std::string& str);

	// wrap an object in a list
	Object build_list(const Object& obj);

	// build a list out of a vector of forms
	Object build_list(const std::vector<Object>& objects);

	// build a list out of an array of forms
	Object build_list(const Object* objects, int count);

	// build a list out of a vector of strings that are converted to symbols
	Object build_list(const std::vector<std::string>& symbols);

	// fancy wrapper functions.  Due to template magic these can call each other
	// and accept mixed arguments!

	template <typename... Args>
	Object build_list(const Object& car, Args... rest);

	template <typename... Args>
	Object build_list(const std::string& str, Args... rest) {
		return Object::make_pair(to_symbol(str), build_list(rest...));
	}

	template <typename... Args>
	Object build_list(const Object& car, Args... rest) {
		return Object::make_pair(car, build_list(rest...));
	}

	Reader& get_pretty_printer_reader();

	Object float_representation(float value);

	void append(Object& _in, const Object& add);
}  // namespace pretty_print
