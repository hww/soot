// print_float.h
#pragma once
#include <string>
#include <cstdint>

std::string float_to_string(float value, bool append_trailing_decimal = true);
std::string meters_to_string(float value, bool append_trailing_decimal = false);
std::string degrees_to_string(float value, bool append_trailing_decimal = false);
int float_to_cstr(float value, char* buffer, bool append_trailing_decimal = true);
bool proper_float(float value);