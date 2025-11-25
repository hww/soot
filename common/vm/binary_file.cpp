#include "binary_file.hpp"
#include <format>

namespace vm {

    void BinaryFile::relocate_pointers(void* pool_base) {
        ASSERT (pool_base != nullptr);


        // Вычисляем старый базовый адрес пула
        auto new_offset = reinterpret_cast<uintptr_t>(this) - reinterpret_cast<uintptr_t>(pool_base);
        auto delta = new_offset - base_offset;
        base_offset = new_offset;

        lg::info("relocating file by offset: {}", delta);
        
        if (delta == 0) return;

        // Обновляем base_offset для нового положения
        generation++;

        // Релоцируем указатели в заголовке
        definitions.offset += delta;

        // Релоцируем указатели в определениях
        for (u32 i = 0; i < definitions_count; i++) {
            Definition* def = get_definition(i);
            def->data_ptr.offset += delta;

            // ЕСЛИ определение - функция, релоцируем и её внутренние указатели
            if (def->type == type::function) {
                Descriptor* desc = def->data_ptr.cast<Descriptor>().c();
                if (desc) {
                    desc->relocate_pointers(delta);
                }
            }
        }
    }
}