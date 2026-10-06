#pragma once

#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"

namespace carbon {

    /// @brief Descriptor of one known SsDeclaration type.
    /// @details Maps a type SID (e.g. SID("vector")) to its byte size on disk.
    ///          The size is the number of bytes at m_pDeclValue that hold the value.
    ///          Types are matched by SID64, not by enum.
    struct DeclarationTypeInfo {
        sid64       type_sid;   ///< SID64 of the type name
        const char *name;       ///< human-readable name
        u32         size_bytes; ///< size of the value block in bytes
    };

    /// @brief Table of all known SsDeclaration types.
    /// @details Built from observed state-script files in T2R / T1X.
    ///          Types not in this table are reported as "???" by the inspector.
    inline constexpr DeclarationTypeInfo KNOWN_DECL_TYPES[] = {
        {SID("boolean"), "boolean", 1},         ///< u8
        {SID("int32"), "int32", 4},             ///< i32
        {SID("uint64"), "uint64", 8},           ///< u64
        {SID("float"), "float", 4},             ///< f32
        {SID("timer"), "timer", 4},             ///< f32
        {SID("bound-frame"), "bound-frame", 4}, ///< f32
        {SID("symbol"), "symbol", 8},           ///< sid64
        {SID("string"), "string", 8},           ///< const char* (pointer, relocated)
        {SID("vector"), "vector", 16},          ///< 4 x f32
        {SID("quat"), "quat", 16},              ///< 4 x f32
        {SID("point"), "point", 12},            ///< 3 x f32
    };

    /// @brief Look up a declaration type by its SID64.
    /// @return pointer to the descriptor, or nullptr if the type is not known.
    [[nodiscard]] inline const DeclarationTypeInfo *find_decl_type(sid64 type_sid) noexcept {
        for (const auto &info : KNOWN_DECL_TYPES) {
            if (info.type_sid == type_sid) { return &info; }
        }
        return nullptr;
    }

} // namespace carbon