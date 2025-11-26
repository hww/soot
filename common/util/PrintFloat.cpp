// print_float.cpp
#include "common/util/PrintFloat.hpp"
#include <cmath>
#include <cstring>

static constexpr float METER_LENGTH = 1.0f;
static constexpr float DEGREES_LENGTH = 1.0f;

std::string float_to_string(float value, bool append_trailing_decimal) {
    constexpr int buff_size = 128;
    char buff[buff_size];
    float_to_cstr(value, buff, append_trailing_decimal);
    return { buff };
}

std::string meters_to_string(float value, bool append_trailing_decimal) {
    return float_to_string(value / METER_LENGTH, append_trailing_decimal);
}

std::string degrees_to_string(float value, bool append_trailing_decimal) {
    return float_to_string(value / DEGREES_LENGTH, append_trailing_decimal);
}

int float_to_cstr(float value, char* buffer, bool append_trailing_decimal) {
    if (!std::isfinite(value)) {
        if (std::isnan(value)) {
            std::strcpy(buffer, "nan");
            return 3;
        }
        else {
            std::strcpy(buffer, value > 0 ? "inf" : "-inf");
            return value > 0 ? 3 : 4;
        }
    }

    int i = 0;

    if (value == 0.0f) {
        buffer[i++] = '0';
        if (append_trailing_decimal) {
            buffer[i++] = '.';
            buffer[i++] = '0';
        }
        buffer[i] = '\0';
        return i;
    }

    // Handle sign
    if (value < 0) {
        buffer[i++] = '-';
        value = -value;
    }

    // Extract integer and fractional parts
    double integer_part;
    double fractional_part = std::modf(value, &integer_part);

    // Convert integer part
    uint64_t integer_val = static_cast<uint64_t>(integer_part);
    char int_buf[64];
    int int_len = 0;

    if (integer_val == 0) {
        int_buf[int_len++] = '0';
    }
    else {
        uint64_t temp = integer_val;
        while (temp > 0) {
            int_buf[int_len++] = '0' + (temp % 10);
            temp /= 10;
        }
        // Reverse
        for (int j = 0; j < int_len / 2; j++) {
            std::swap(int_buf[j], int_buf[int_len - 1 - j]);
        }
    }

    // Copy integer part to buffer
    for (int j = 0; j < int_len; j++) {
        buffer[i++] = int_buf[j];
    }

    // Handle fractional part
    if (fractional_part != 0.0 || append_trailing_decimal) {
        buffer[i++] = '.';

        if (fractional_part == 0.0) {
            buffer[i++] = '0';
        }
        else {
            // Convert fractional part with limited precision
            double frac = fractional_part;
            int frac_digits = 0;
            const int max_frac_digits = 10;

            while (frac > 0 && frac_digits < max_frac_digits) {
                frac *= 10.0;
                int digit = static_cast<int>(frac);
                buffer[i++] = '0' + digit;
                frac -= digit;
                frac_digits++;
            }

            // Remove trailing zeros
            while (i > 0 && buffer[i - 1] == '0') {
                i--;
            }

            // If we removed all fractional digits, keep at least one zero if requested
            if (buffer[i - 1] == '.') {
                if (append_trailing_decimal) {
                    buffer[i++] = '0';
                }
                else {
                    i--; // Remove the decimal point
                }
            }
        }
    }

    buffer[i] = '\0';
    return i;
}

bool proper_float(float value) {
    if (!std::isfinite(value)) {
        return false;
    }

    uint32_t int_value;
    std::memcpy(&int_value, &value, 4);
    uint8_t exp = (int_value >> 23) & 0xFF;
    uint32_t mant = int_value & 0x7FFFFF;

    // Check for denormalized numbers
    if (exp == 0 && mant != 0) {
        return false;
    }

    return true;
}