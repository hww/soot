#pragma once

#include "CommonTypes.hpp"
#include "common/carbon/file/ProgramBinaryElement.hpp"
#include <vector>
#include <map>

using namespace carbon;
namespace sootc {

class StaticSegment {
public:
    using SlotIndex = u32;

    // Добавляем значения в "таблицу констант" (slots)
    SlotIndex add_int32(int32_t value) {
        slots.push_back(static_cast<u32>(value));
        return static_cast<SlotIndex>(slots.size() - 1);
    }

    SlotIndex add_float(float value) {
        u32 bits;
        std::memcpy(&bits, &value, 4);
        slots.push_back(bits);
        return static_cast<SlotIndex>(slots.size() - 1);
    }

    SlotIndex add_string_pointer(const std::string& str) {
        if (auto it = string_cache.find(str); it != string_cache.end()) {
            return it->second;
        }

        SlotIndex slot_idx = static_cast<SlotIndex>(slots.size());
        // Резервируем место под оффсет (будет запатчен релокацией)
        slots.push_back(0); 

        // Записываем строку в raw_buffer
        size_t str_offset = raw_buffer.size();
        const char* c_str = str.c_str();
        raw_buffer.insert(raw_buffer.end(), c_str, c_str + str.size() + 1);

        // Запоминаем, что слот slot_idx должен указывать на str_offset внутри raw_buffer
        pending_string_relocs.push_back({slot_idx, static_cast<u32>(str_offset)});
        
        string_cache[str] = slot_idx;
        return slot_idx;
    }

    // Главный метод: переносит всё содержимое в итоговый RelocatableBuffer
    void emit_to(ProgramBinaryElement& final_buffer) {
        size_t base_offset = final_buffer.size();

        // 1. Записываем таблицу слотов
        for (u64 val : slots) {
            final_buffer.push_bytes(val,0);
        }

        // 2. Записываем сырые данные (строки и т.д.)
        size_t raw_data_start = final_buffer.size();
        final_buffer.push_blob(raw_buffer.data(), raw_buffer.size());

        // 3. Патчим оффсеты строк внутри уже записанных слотов
        // Теперь оффсет в слоте будет указывать точно на начало строки относительно начала блока данных
        for (auto& reloc : pending_string_relocs) {
            u32 final_str_pos = static_cast<u32>(raw_data_start - base_offset + reloc.raw_offset);
            // Патчим записанный ранее u32 в final_buffer
            // FIX IT  final_buffer.patch_u32(base_offset + (reloc.slot_idx * 4), final_str_pos);
        }
    }

    size_t total_size() const {
        return (slots.size() * 4) + raw_buffer.size();
    }

private:
    struct StringReloc {
        u32 slot_idx;
        u32 raw_offset;
    };

    std::vector<u32> slots;
    std::vector<u8> raw_buffer;
    std::map<std::string, SlotIndex> string_cache;
    std::vector<StringReloc> pending_string_relocs;
};

} // namespace sootc