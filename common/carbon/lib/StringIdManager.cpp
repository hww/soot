// StringIdManager.cpp
#include "common/carbon/lib/StringIdManager.hpp"
#include "common/carbon/defconstruct/include/sidbase.h"
#include "common/util/Log.hpp"
#include "fmt/format.h"
#include "util/Crc32.hpp"
#include <algorithm>
#include <fstream>
#include <mutex>
#include <sstream>
#include <shared_mutex>
#include <iomanip>
#include <cctype>

namespace carbon::lib {

// ============================================================================
// Registration (for reverse lookup only)
// ============================================================================

u32 StringIdManager::register_string(const std::string& str) {
    u32 id = util::compute_crc32(str);
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = reverse_lookup_.find(id);
    if (it != reverse_lookup_.end()) {
        if (it->second != str) {
            lg::error("StringIdManager: CRC32 collision detected! ID 0x{:08X} for both '{}' and '{}'",
                id, it->second, str);
        }
        return id;
    }
    
    reverse_lookup_[id] = str;
    lg::debug("StringIdManager: registered '{}' as ID 0x{:08X}", str, id);
    return id;
}

u32 StringIdManager::register_string(const char* str) {
    if (!str || *str == '\0') {
        return 0;
    }
    return register_string(std::string(str));
}

// ============================================================================
// Lookup
// ============================================================================

std::string StringIdManager::get_string(u32 id) const {
    if (id == 0) {
        return "";
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = reverse_lookup_.find(id);
    if (it != reverse_lookup_.end()) {
        return it->second;
    }
    
    // Формат: <unknown:0xDEADBEEF>
    return fmt::format("<unknown:0x{:08X}>", id);
}

const char* StringIdManager::get_cstring(u32 id) const {
    // Используем thread_local буфер для возврата указателя
    static thread_local std::string buffer;
    buffer = get_string(id);
    return buffer.c_str();
}

bool StringIdManager::has_string(u32 id) const {
    if (id == 0) {
        return false;
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return reverse_lookup_.find(id) != reverse_lookup_.end();
}

// ============================================================================
// Helper: parse hex string to u32
// ============================================================================

static u32 parse_hex(const std::string& hex_str) {
    u32 value = 0;
    std::stringstream ss;
    ss << std::hex << hex_str;
    ss >> value;
    return value;
}

static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// ============================================================================
// Serialization (text format: CRC32 name)
// ============================================================================

bool StringIdManager::save_table(const std::string& filename) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        lg::error("StringIdManager: cannot open file '{}' for writing", filename);
        return false;
    }
    
    for (const auto& [id, str] : reverse_lookup_) {
        file << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << id 
             << " " << str << "\n";
        
        if (!file.good()) {
            lg::error("StringIdManager: error writing to file '{}'", filename);
            return false;
        }
    }
    
    file.close();
    lg::debug("StringIdManager: saved {} entries to '{}'", reverse_lookup_.size(), filename);
    return true;
}

// StringIdManager.cpp - исправленная версия load_table
bool StringIdManager::load_table(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        lg::warn("StringIdManager: cannot open file '{}' for reading", filename);
        return false;
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    reverse_lookup_.clear();
    
    std::string line;
    int line_num = 0;
    int valid_entries = 0;
    
    while (std::getline(file, line)) {
        line_num++;
        
        // Удаляем пробелы в начале и конце
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue; // Пустая строка
        
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        
        // Находим разделитель (пробел или табуляция)
        size_t space_pos = line.find_first_of(" \t");
        if (space_pos == std::string::npos) {
            lg::error("StringIdManager: invalid format at line {}: '{}'", line_num, line);
            continue;
        }
        
        std::string hex_str = line.substr(0, space_pos);
        std::string name = line.substr(space_pos + 1);
        
        // Удаляем пробелы в имени
        start = name.find_first_not_of(" \t\r\n");
        if (start != std::string::npos) {
            end = name.find_last_not_of(" \t\r\n");
            name = name.substr(start, end - start + 1);
        } else {
            name = "";
        }
        
        // Проверяем hex формат
        if (hex_str.empty()) {
            lg::error("StringIdManager: empty hex value at line {}", line_num);
            continue;
        }
        
        bool valid_hex = true;
        for (char c : hex_str) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                valid_hex = false;
                break;
            }
        }
        
        if (!valid_hex) {
            lg::error("StringIdManager: invalid hex value at line {}: '{}'", line_num, hex_str);
            continue;
        }
        
        if (name.empty()) {
            lg::error("StringIdManager: empty name at line {}", line_num);
            continue;
        }
        
        // Парсим hex
        u32 id = 0;
        std::stringstream ss;
        ss << std::hex << hex_str;
        ss >> id;
        
        reverse_lookup_[id] = name;
        valid_entries++;
    }
    
    file.close();
    lg::debug("StringIdManager: loaded {} valid entries from '{}' (total lines: {})", 
              valid_entries, filename, line_num);
    return valid_entries > 0;
}

// ============================================================================
// Inspection
// ============================================================================

std::string StringIdManager::inspect() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "StringIdManager: " << reverse_lookup_.size() << " entries\n";
    
    size_t max_to_show = 20;
    size_t shown = 0;
    
    for (const auto& [id, str] : reverse_lookup_) {
        if (shown >= max_to_show) {
            oss << "  ... and " << (reverse_lookup_.size() - shown) << " more\n";
            break;
        }
        oss << "  0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << id 
            << std::dec << " = \"" << str << "\"\n";
        ++shown;
    }
    
    return oss.str();
}

size_t StringIdManager::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return reverse_lookup_.size();
}

void StringIdManager::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    reverse_lookup_.clear();
    lg::debug("StringIdManager: cleared all entries");
}
// ============================================================================
// Inspection
// ============================================================================

// Экспорт в бинарный формат dconstruct
bool StringIdManager::save_dconstruct_sidbase(const std::string& filename) const {
    std::shared_lock lock(mutex_);
    
    // Собираем все записи
    std::vector<dconstruct::SIDBaseEntry> entries;
    entries.reserve(reverse_lookup_.size());
    
    // Сначала нужно собрать строки и вычислить офсеты
    std::vector<char> string_pool;
    
    for (const auto& [id, name] : reverse_lookup_) {
        dconstruct::SIDBaseEntry entry;
        entry.hash = id;  // ВАЖНО: преобразовать 32→64 бит
        entry.offset = string_pool.size();
        entries.push_back(entry);
        
        // Добавляем строку с нуль-терминатором
        string_pool.insert(string_pool.end(), name.begin(), name.end());
        string_pool.push_back('\0');
    }
    
    // Сортируем по hash для бинарного поиска
    std::sort(entries.begin(), entries.end(), 
              [](const auto& a, const auto& b) { return a.hash < b.hash; });
    
    // Пишем файл
    std::ofstream file(filename, std::ios::binary);
    u64 num_entries = entries.size();
    file.write(reinterpret_cast<const char*>(&num_entries), 8);
    file.write(reinterpret_cast<const char*>(entries.data()), 
               entries.size() * sizeof(dconstruct::SIDBaseEntry));
    file.write(string_pool.data(), string_pool.size());
    
    return true;
}

// Загрузка из бинарного формата dconstruct
bool StringIdManager::load_dconstruct_sidbase(const std::string& filename) {
    auto result = dconstruct::SIDBase::from_binary(filename);
    if (!result) {
        return false;
    }
    
    auto& sidbase = *result;
    
    std::unique_lock lock(mutex_);
    reverse_lookup_.clear();
    
    // Итерируем по всем записям
    for (u64 i = 0; i < sidbase.numEntries(); ++i) {
        const auto& entry = sidbase[i];
        const char* name = sidbase.get_string_by_offset(entry.offset);
        
        // Преобразуем 64→32 бит (если влазит)
        u32 id32 = static_cast<u32>(entry.hash);
        reverse_lookup_[id32] = name;
    }
    
    return true;
}

} // namespace carbon::lib