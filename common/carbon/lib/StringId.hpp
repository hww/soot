// StringId.hpp
#pragma once

#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringIdManager.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace carbon {

    /// @brief Lightweight wrapper around a SID64 value with automatic registration.
    /// @details Constructing from a string registers it in the global StringIdManager
    ///          so that the string can be recovered later (for disassembly, logging,
    ///          and debug output). Constructing from an integer does NOT register.
    class StringId {
    public:
        sid64 value; ///< raw SID64

        constexpr StringId() : value(0) {}
        constexpr StringId(u64 val) : value(val) {}
        constexpr StringId(i64 val) : value(static_cast<u64>(val)) {}
        constexpr StringId(u32 val) : value(val) {}
        constexpr StringId(i32 val) : value(static_cast<u64>(val)) {}

        StringId(const char *str) : value(StringIdManager::instance().register_string(str)) {}
        StringId(const std::string &str)
            : value(StringIdManager::instance().register_string(str)) {}

        constexpr operator u64() const { return value; }

        constexpr bool operator==(const StringId &other) const { return value == other.value; }
        constexpr bool operator!=(const StringId &other) const { return value != other.value; }

        /// @return the registered string for this SID, or "<unknown:0x...>" if not registered.
        std::string to_string() const { return StringIdManager::instance().get_string(value); }

        /// @return a thread-local C-string for this SID.
        const char *to_cstring() const { return StringIdManager::instance().get_cstring(value); }

        /// @return a short debug representation (used by logging).
        const char *debug_str() const;

        static const StringId None;
        static const StringId Null;
    };

    /// @brief Pre-registered StringId constants used across the codebase.
    struct StringIds {
        inline static const StringId none = StringId("none");
        inline static const StringId unknown = StringId("unknown");
        inline static const StringId unnamed = StringId("unnamed");
        inline static const StringId enter = StringId("enter");
        inline static const StringId exit = StringId("exit");
        inline static const StringId trans = StringId("trans");
        inline static const StringId event = StringId("event");
        inline static const StringId post = StringId("post");
        inline static const StringId code = StringId("code");
        inline static const StringId script_lambda = StringId("script-lambda");
    };

} // namespace carbon


/// @brief Free-function overloads for mixed StringId / sid64 comparisons.
/// @details Without these, `sid64 == StringId(...)` is ambiguous because
///          StringId has an implicit conversion to u64.
inline constexpr bool operator==(const carbon::StringId &lhs, sid64 rhs) noexcept {
    return lhs.value == rhs;
}
inline constexpr bool operator==(sid64 lhs, const carbon::StringId &rhs) noexcept {
    return lhs == rhs.value;
}
inline constexpr bool operator!=(const carbon::StringId &lhs, sid64 rhs) noexcept {
    return lhs.value != rhs;
}
inline constexpr bool operator!=(sid64 lhs, const carbon::StringId &rhs) noexcept {
    return lhs != rhs.value;
}


// ===========================================================================
// SID() macros
// ===========================================================================

#include "common/util/StringIdHash.hpp"

/// @brief Build a StringId from a string literal without runtime registration.
/// @details The hash is computed at compile time; the resulting StringId is
///          NOT registered in StringIdManager. Use only for comparison against
///          constants that are registered elsewhere.
#define SID(str)   (::carbon::StringId(static_cast<::sid64>(util::ToStringId64_Const(str))))
#define SID32(str) (::carbon::StringId(static_cast<::sid64>(util::ToStringId32_Const(str))))


// ===========================================================================
// std::hash specialization
// ===========================================================================

namespace std {
    template <> struct hash<carbon::StringId> {
        size_t operator()(const carbon::StringId &sid) const noexcept {
            return std::hash<uint64_t>{}(sid.value);
        }
    };
} // namespace std


// ===========================================================================
// fmt::formatter specialization
// ===========================================================================

#include "fmt/format.h"

template <> struct fmt::formatter<carbon::StringId> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const carbon::StringId &sid, FormatContext &ctx) const {
        const std::string name = sid.to_string();
        return fmt::format_to(ctx.out(), "{}", name);
    }
};


// ===========================================================================
// std::formatter specialization
// ===========================================================================

#include <format>

namespace std {
    template <> struct formatter<carbon::StringId> {
        constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

        auto format(const carbon::StringId &sid, format_context &ctx) const {
            const std::string name = sid.to_string();
            return std::format_to(ctx.out(), "{}", name);
        }
    };
} // namespace std