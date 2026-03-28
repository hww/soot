// common/carbon/files/RelocatableBuffer.hpp
#pragma once

#include <cstddef>
#include <vector>
#include <cstring>
#include "common/CommonTypes.hpp"
#include "files/Definition.hpp"

namespace carbon::files {

class RelocatableBuffer {
public:
    // Добавить сырые байты
    void add_bytes(const void* data, size_t size) {
        const u8* ptr = reinterpret_cast<const u8*>(data);
        bytes_.insert(bytes_.end(), ptr, ptr + size);
    }
    
    // Добавить значение с возможностью релокации
    void add_value(u64 value, bool is_relocatable = false) {
        u32 offset = bytes_.size();
        add_bytes(&value, sizeof(u64));
        if (is_relocatable) {
            relocatable_offsets_.push_back(offset);
        }
    }
    
    // Добавить указатель (смещение)
    void add_ptr(u64 offset, bool is_relocatable = true) {
        add_value(offset, is_relocatable);
    }
    
    // Отметить текущую позицию как требующую релокации
    void mark_current_as_relocatable() {
        relocatable_offsets_.push_back(bytes_.size());
        add_bytes(&dummy_, sizeof(u64));  // временное место
    }
    
    // Добавить подбуфер (для вложенных структур)
    void add_buffer(const RelocatableBuffer& other, u64 base_offset = 0) {
        u32 insert_pos = bytes_.size();
        bytes_.insert(bytes_.end(), other.bytes_.begin(), other.bytes_.end());
        
        // Смещаем relocatable offsets из подбуфера
        for (u32 offset : other.relocatable_offsets_) {
            relocatable_offsets_.push_back(insert_pos + offset);
        }
    }
    
    const std::vector<u8>& bytes() const { return bytes_; }
    const std::vector<u32>& relocatable_offsets() const { return relocatable_offsets_; }
    
    size_t size() const { return bytes_.size(); }
    
    bool is_empty() const { return size()!=0; }

    void add_relocatable_offset(u32 offset) {
        relocatable_offsets_.push_back(offset);
    }
    
    u8* data() { return bytes_.data(); }
    const u8* data() const { return bytes_.data(); }

    std::string inspect() const {
        const int MAX_LEN = 8;
        std::string result = " [";
        for (size_t j = 0; j < std::min(size(), size_t(MAX_LEN)); j++) {
            result += fmt::format("{:02x}", bytes_[j]);
        }
        if (size() > MAX_LEN) {
            result += "...";
        }
        result += "]";
        return result;
    }

    /** Добавить функцию */
    void add_function(
            const std::vector<vm::Instruction>& code,
            const std::vector<u8>& data = {},
            const std::vector<SourceLocation>& debug_info = {},
            SymbolFlags flags = SymbolFlags::Export);

private:
    std::vector<u8> bytes_;
    std::vector<u32> relocatable_offsets_;
    u64 dummy_ = 0;
};

} // namespace carbon::files