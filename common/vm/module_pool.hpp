#pragma once

#include "types.hpp"
#include <vector>
#include <memory>
#include <cstring>

namespace vm {
    
    extern u8* g_module_pool_base;

    class ModulePool {
    private:
        static inline u8* pool_base = nullptr;
        static inline u32 pool_size = 0;
        static inline u32 current_offset = 0;
        static inline std::vector<u32> module_offsets;

    public:
        // Запрещаем копирование и создание экземпляров
        ModulePool() = delete;
        ModulePool(const ModulePool&) = delete;
        ModulePool& operator=(const ModulePool&) = delete;

        // Инициализация пула фиксированного размера
        static bool initialize(u32 total_size) {
            if (pool_base != nullptr) {
                shutdown(); // Переинициализация
            }

            pool_base = new u8[total_size];
            if (!pool_base) return false;

            pool_size = total_size;
            current_offset = 0;
            module_offsets.clear();

            // Инициализируем нулями для детерминированности
            std::memset(pool_base, 0, total_size);
            return true;
        }

        // Загрузка модуля в пул
        static void* load_module(const std::vector<u8>& data) {
            if (!pool_base || data.empty()) return nullptr;

            u32 size = static_cast<u32>(data.size());
            u32 aligned_size = align_size(size);

            // Проверяем хватит ли места
            if (current_offset + aligned_size > pool_size) {
                return nullptr;
            }

            u32 module_offset = current_offset;
            module_offsets.push_back(module_offset);

            // Копируем данные
            std::memcpy(pool_base + module_offset, data.data(), size);

            current_offset += aligned_size;
            return pool_base + module_offset;
        }

        // Загрузка с релокацией
        static void* load_and_relocate_module(std::vector<u8>&& data) {
            void* module_addr = load_module(data);
            if (!module_addr) return nullptr;

            relocate_module(module_addr, static_cast<u32>(data.size()));
            return module_addr;
        }

        // Релокация модуля
        static void relocate_module(void* module_start, u32 module_size) {
            if (!module_start || !pool_base) return;

            u8* module_base = static_cast<u8*>(module_start);
            u32 pool_offset = static_cast<u32>(module_base - pool_base);

            // Проверяем что модуль действительно в нашем пуле
            if (pool_offset >= pool_size) return;

            // Здесь будет сложная логика релокации, пока заглушка
            // TODO: Реализовать полную релокацию структур
        }

        // Базовый адрес пула
        static void* get_base_address() { return pool_base; }

        // Получить адрес модуля по индексу
        static void* get_module_address(u32 index) {
            if (index >= module_offsets.size()) return nullptr;
            return pool_base + module_offsets[index];
        }

        // Освобождение пула
        static void shutdown() {
            delete[] pool_base;
            pool_base = nullptr;
            pool_size = 0;
            current_offset = 0;
            module_offsets.clear();
        }

        // Статистика
        static bool is_initialized() { return pool_base != nullptr; }
        static u32 get_pool_size() { return pool_size; }
        static u32 get_used_memory() { return current_offset; }
        static u32 get_free_memory() { return pool_size - current_offset; }
        static u32 get_module_count() { return static_cast<u32>(module_offsets.size()); }
        static double get_utilization() {
            return pool_size > 0 ? (static_cast<double>(current_offset) / pool_size) * 100.0 : 0.0;
        }

        // Утилиты
        static constexpr u32 align_size(u32 n) {
            return (n + 3) & ~3;
        }
    };

} // namespace vm