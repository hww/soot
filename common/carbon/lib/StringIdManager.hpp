// StringIdManager.hpp
#pragma once

#include "common/CommonTypes.hpp"
#include <string>
#include <unordered_map>
#include <shared_mutex>

namespace carbon::lib {

class StringIdManager {
public:
    static StringIdManager& instance() {
        static StringIdManager inst;
        return inst;
    }

    // Регистрация строки (вычисляет CRC32 и сохраняет для обратного поиска)
    u32 register_string(const std::string& str);
    u32 register_string(const char* str);

    // Получение строки по ID (CRC32)
    std::string get_string(u32 id) const;
    const char* get_cstring(u32 id) const;

    // Проверка наличия
    bool has_string(u32 id) const;

    // Сериализация таблицы обратного поиска
    bool save_table(const std::string& filename) const;
    bool load_table(const std::string& filename);

    // Сериализация бинарного файла
    bool load_dconstruct_sidbase(const std::string& filename);
    bool save_dconstruct_sidbase(const std::string& filename) const;

    // Инспекция
    std::string inspect() const;
    size_t size() const;

    // Итераторы
    using const_iterator = std::unordered_map<u32, std::string>::const_iterator;
    const_iterator begin() const { return reverse_lookup_.begin(); }
    const_iterator end() const { return reverse_lookup_.end(); }

    // Очистка
    void clear();

private:
    StringIdManager() = default;
    mutable std::shared_mutex mutex_;
    std::unordered_map<u32, std::string> reverse_lookup_;  // ID -> string
};

} // namespace carbon::lib