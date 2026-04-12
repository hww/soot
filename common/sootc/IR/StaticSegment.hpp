#pragma once

#include "CommonTypes.hpp"
#include <vector>
#include <map>

namespace sootc {

class StaticSegment {
public:
    using SlotIndex = u32; // Индекс kk в командах ВМ
    using Offset = u32;    // Байтовое смещение внутри пула

    // Добавление I32/U32 - занимает 1 слот
    SlotIndex add_int32(int32_t value) {
        return add_to_slots(static_cast<u32>(value));
    }

    // Добавление Float - занимает 1 слот
    SlotIndex add_float(float value) {
        u32 bits;
        std::memcpy(&bits, &value, 4);
        return add_to_slots(bits);
    }

    // Добавление StringId (хэша) - занимает 1 слот
    SlotIndex add_string_id(u32 hash) {
        return add_to_slots(hash);
    }

SlotIndex add_string_pointer(const std::string& str) {
        if (auto it = string_cache.find(str); it != string_cache.end()) {
            return it->second;
        }

        SlotIndex slot_idx = static_cast<SlotIndex>(slots.size());
        slots.push_back(0); 

        // Выравнивание для строк обычно 1, но для других данных может быть больше
        Offset str_offset = add_to_raw_buffer(str.c_str(), str.size() + 1, 1);
        
        slots[slot_idx] = static_cast<u32>(str_offset);
        string_cache[str] = slot_idx;
        return slot_idx;
    }

    std::vector<u8> finalize() {
        std::vector<u8> result;
        result.reserve(slots.size() * 4 + raw_buffer.size());
        for (u32 val : slots) {
            u8 bytes[4];
            std::memcpy(bytes, &val, 4);
            result.insert(result.end(), bytes, bytes + 4);
        }
        result.insert(result.end(), raw_buffer.begin(), raw_buffer.end());
        return result;
    }

private:
    std::vector<u32> slots;
    std::vector<u8> raw_buffer;
    std::map<std::string, SlotIndex> string_cache;

    SlotIndex add_to_slots(u32 val) {
        slots.push_back(val);
        return static_cast<SlotIndex>(slots.size() - 1);
    }

    Offset add_to_raw_buffer(const void* data, size_t size, size_t alignment) {
        // Чинним варнинг и добавляем реальное выравнивание
        size_t current_pos = slots.size() * 4 + raw_buffer.size();
        size_t padding = (alignment - (current_pos % alignment)) % alignment;
        
        for (size_t i = 0; i < padding; ++i) {
            raw_buffer.push_back(0);
        }

        Offset offset = static_cast<Offset>(slots.size() * 4 + raw_buffer.size());
        const u8* ptr = reinterpret_cast<const u8*>(data);
        raw_buffer.insert(raw_buffer.end(), ptr, ptr + size);

        return offset;
    }
};

} // namespace sootc