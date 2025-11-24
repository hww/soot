#include "binary_file.hpp"
#include <format>

namespace vm {

    void BinaryFile::relocate_pointers(void* pool_base) {
        if (!pool_base) return;

        lg::info("=== RELOCATION DEBUG ===");
        lg::info("this: {}, pool_base: {}, base_offset: {}", (void*)this, pool_base, base_offset);

        // Вычисляем старый базовый адрес пула
        auto new_offset = reinterpret_cast<uintptr_t>(this) - reinterpret_cast<uintptr_t>(pool_base);
        auto delta = new_offset - base_offset;


        // ПРОВЕРИМ ДАННЫЕ ДО релокации
        lg::info("BEFORE RELOCATION - definitions table:");
        for (u32 i = 0; i < definitions_count; i++) {
            Definition* def = (Definition*)((uintptr_t)this + definitions.offset + i * sizeof(Definition));
            lg::info("  def[{}]: name={}, type={}, data_ptr={}",
                i, def->name, def->type, def->data_ptr.offset);
        }

        // Релоцируем указатели в заголовке
        definitions = relocate_single_ptr(definitions, delta);

        // Релоцируем указатели в определениях
        for (u32 i = 0; i < definitions_count; i++) {
            Definition* def = get_definition(i);

            // ПРОВЕРИМ ДАННЫЕ ПОСЛЕ получения указателя
            lg::info("def[{}] before data_ptr relocation: name={}, type={}, data_ptr={}",
                i, def->name, def->type, def->data_ptr.offset);

            def->data_ptr = relocate_single_ptr(def->data_ptr, delta);

            // ПРОВЕРИМ ДАННЫЕ ПОСЛЕ релокации
            lg::info("def[{}] after data_ptr relocation: name={}, type={}, data_ptr={}",
                i, def->name, def->type, def->data_ptr.offset);

            // ЕСЛИ определение - функция, релоцируем и её внутренние указатели
            if (def->type == type::function) {
                Descriptor* desc = def->data_ptr.cast<Descriptor>().c();
                if (desc) {
                    lg::info("Relocating ByteCode for function");
                    desc->relocate_pointers(delta);
                }
            }
        }

        // Обновляем base_offset для нового положения
        base_offset = new_offset;
        generation++;

        // ПРОВЕРИМ ДАННЫЕ ПОСЛЕ всей релокации
        lg::info("AFTER RELOCATION - definitions table:");
        for (u32 i = 0; i < definitions_count; i++) {
            Definition* def = get_definition(i);
            lg::info("  def[{}]: name={}, type={}, data_ptr={}",
                i, def->name, def->type, def->data_ptr.offset);
        }
    }
}