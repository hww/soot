// StringId.hpp
#pragma once

#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringIdManager.hpp"

namespace carbon::lib {

class StringId {
public:
    u32 value;

    constexpr StringId() : value(0) {}
    constexpr explicit StringId(u32 val) : value(val) {}
    
    StringId(const char* str) : value(StringIdManager::instance().register_string(str)) {}
    StringId(const std::string& str) : value(StringIdManager::instance().register_string(str)) {}

    constexpr operator u32() const { return value; }
    constexpr bool operator==(const StringId& other) const { return value == other.value; }
    constexpr bool operator!=(const StringId& other) const { return value != other.value; }

    std::string to_string() const { return StringIdManager::instance().get_string(value); }
    const char* to_cstring() const { return StringIdManager::instance().get_cstring(value); }

    static const StringId None;
    static const StringId Null;
};


 struct StringIds {
    inline static const StringId unknown   = StringId("unknown");
    inline static const StringId unnamed   = StringId("unnamed");
    inline static const StringId enter     = StringId("enter");
    inline static const StringId exit      = StringId("exit");
    inline static const StringId trans     = StringId("trans");
    inline static const StringId event     = StringId("event");
    inline static const StringId post      = StringId("post");
    inline static const StringId code      = StringId("code");
 };


} // namespace carbon::lib


/**
 * Main macro for creating StringId from string literals
 * Used in code, generator tool finds these calls
 * Example: SID("player") -> CRC32 of "player"
 */
#include "common/util/Crc32.hpp"

#define SID(str) (::carbon::lib::StringId(::util::compute_crc32_constexpr(str)))


// 3. РАСШИРЕНИЕ СТАНДАРТНОЙ БИБЛИОТЕКИ
#include <functional> // Обязательно для std::hash

namespace std {
    template <>
    struct hash<carbon::lib::StringId> {
        size_t operator()(const carbon::lib::StringId& sid) const noexcept {
            // Просто используем стандартный хеш для u32
            return std::hash<u32>{}(sid.value);
        }
    };
}

// 4. РАСШИРЕНИЕ FMT
// Make string ID supportable by formatter
#include "fmt/format.h"

template <>
struct fmt::formatter<carbon::lib::StringId> {
    // Парсим формат (например, {:x} для hex или {:s} для строки)
    // По умолчанию будем выводить строку, если она есть
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const carbon::lib::StringId& sid, FormatContext& ctx) const {
        // Пытаемся получить имя из глобальной таблицы
        const char* name = sid.to_cstring();
        
        if (std::string(name) == "<unknown>") {
            // Если имени нет, выводим HEX-значение для дебага
            return fmt::format_to(ctx.out(), "ID(0x{:08X})", (u32)sid);
        }
        
        // Если имя есть, выводим его
        return fmt::format_to(ctx.out(), "{}", name);
    }
};

// 4. РАСШИРЕНИЕ FORMAT
#include <format> // Для std::formatter

namespace std {
    template <>
    struct formatter<carbon::lib::StringId> {
        // Парсим формат
        constexpr auto parse(format_parse_context& ctx) {
            return ctx.begin();
        }

        // Форматируем
        auto format(const carbon::lib::StringId& sid, format_context& ctx) const {
            const char* name = sid.to_cstring();
            
            if (std::string(name) == "<unknown>") {
                // Выводим HEX, если строка не зарегистрирована
                return std::format_to(ctx.out(), "ID(0x{:08x})", static_cast<u32>(sid));
            }
            
            return std::format_to(ctx.out(), "{}", name);
        }
    };
}