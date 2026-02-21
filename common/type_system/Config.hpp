#pragma once

#include "common/CommonTypes.hpp"
#include <string>

// ============================================================================
// Common Constants
// ============================================================================

enum class RegClass { GPR_8, GPR_16, GPR_32, GPR_64, FPR, INVALID };

std::string reg_kind_to_string(RegClass reg_class);

constexpr u32 SOOT_NEW_METHOD = 0;      // method ID of GOAL new
constexpr u32 SOOT_DEL_METHOD = 1;      // method ID of GOAL delete
constexpr u32 SOOT_PRINT_METHOD = 2;    // method ID of GOAL print
constexpr u32 SOOT_INSPECT_METHOD = 3;  // method ID of GOAL inspect
constexpr u32 SOOT_LENGTH_METHOD = 4;   // method ID of GOAL length
constexpr u32 SOOT_ASIZE_METHOD = 5;    // method ID of GOAL size
constexpr u32 SOOT_COPY_METHOD = 6;     // method ID of GOAL copy
constexpr u32 SOOT_RELOC_METHOD = 7;    // method ID of GOAL relocate
constexpr u32 SOOT_MEMUSAGE_METHOD = 8; // method ID of GOAL mem-usage

struct TypeConfig {
    static RegClass pointer_reg_class;
    static int      pointer_size;
    static int      array_data_offset;
    static int      default_alignment;
    static int      crc_value_size;
    static int      struct_alignment;
    static int      struct_array_stride_alignment;
    static int      struct_array_start_alignment;
    static int      basic_array_start_alignment;
};

inline RegClass TypeConfig::pointer_reg_class = RegClass::GPR_64;
inline int      TypeConfig::pointer_size = 4;
inline int      TypeConfig::array_data_offset = 12;
inline int      TypeConfig::default_alignment = 4;
inline int      TypeConfig::crc_value_size = 4;
inline int      TypeConfig::struct_alignment = 16;
inline int      TypeConfig::struct_array_stride_alignment = 16;
inline int      TypeConfig::struct_array_start_alignment = 16;
inline int      TypeConfig::basic_array_start_alignment = 16;
