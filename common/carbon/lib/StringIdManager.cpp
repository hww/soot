#include "common/CommonTypes.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace carbon::lib {

class StringIdManager {
public:
    static StringIdManager& instance() {
        static StringIdManager inst;
        return inst;
    }

    // Регистрация строки
    u32 register_string(const std::string& str);
    u32 register_string(const char* str);

    // Получение строки по ID
    std::string get_string(u32 id) const;
    const char* get_cstring(u32 id) const;

    // Проверка наличия
    bool has_string(u32 id) const;

    // Сериализация
    bool save_table(const std::string& filename) const;
    bool load_table(const std::string& filename);

    // Инспекция
    std::string inspect() const;
    size_t size() const;

    // Итераторы для обхода
    using const_iterator = std::unordered_map<u32, std::string>::const_iterator;
    const_iterator begin() const { return table_.begin(); }
    const_iterator end() const { return table_.end(); }

    // Очистка
    void clear();

private:
    StringIdManager() = default;
    mutable std::mutex mutex_;
    std::unordered_map<u32, std::string> table_;
};

} // namespace carbon::lib