#pragma once

#include "types.hpp"
#include "string_id.hpp"
#include "ptr.hpp"
#include <vector>
#include <algorithm>
#include <cstring>
#include <format>

namespace vm {

    // Предварительное объявление класса Module
    class Module;

    /**
     * @class BinaryFilePool
     * @brief Менеджер памяти для хранения бинарных файлов модулей в непрерывном пуле памяти
     *
     * Класс реализует статический пул памяти для эффективного хранения бинарных данных
     * модулей виртуальной машины. Обеспечивает выделение, освобождение и компактификацию памяти.
     * Использует 32-битные смещения вместо указателей для экономии памяти.
     */
    class BinaryFilePool {
    private:
        /**
         * @struct Allocation
         * @brief Структура для отслеживания выделенных блоков памяти
         */
        struct Allocation {
            void* address;          ///< Адрес начала данных в пуле
            u32 size;               ///< Размер выделенного блока (выровненный)
            Module* owner_module;   ///< Указатель на модуль-владелец
            StringId module_name;   ///< Идентификатор имени модуля для поиска

            /**
             * @brief Конструктор аллокации
             * @param addr Адрес выделенной памяти
             * @param sz Размер данных
             * @param owner Указатель на модуль-владелец
             * @param name Идентификатор имени модуля
             */
            Allocation(void* addr, u32 sz, Module* owner, StringId name)
                : address(addr), size(sz), owner_module(owner), module_name(name) {
            }
        };

        // Статические члены класса (единые для всех экземпляров)
        static inline u8* pool_base = nullptr;      ///< Базовый адрес пула памяти
        static inline u32 pool_size = 0;            ///< Общий размер пула в байтах
        static inline u32 current_offset = 0;       ///< Текущее смещение для следующего выделения
        static inline std::vector<Allocation> allocations;  ///< Вектор всех активных аллокаций

        /// @brief Внутреннее смещение для избежания нулевых указателей
        static constexpr u32 INTERNAL_OFFSET = alignof(std::max_align_t);

    public:
        // Запрет копирования и присваивания
        BinaryFilePool() = delete;
        BinaryFilePool(const BinaryFilePool&) = delete;
        BinaryFilePool& operator=(const BinaryFilePool&) = delete;

        /**
         * @brief Инициализация пула памяти
         * @param total_size Общий размер пула в байтах
         * @return true если инициализация успешна, false в случае ошибки
         *
         * Выделяет память размером total_size + INTERNAL_OFFSET для внутренних нужд.
         * Устанавливает базовый адрес пула в g_module_pool_base.
         */
        static bool initialize(u32 total_size);

        /**
         * @brief Выделение памяти в пуле для модуля
         * @param size Требуемый размер памяти в байтах
         * @param owner_module Указатель на модуль-владелец
         * @param module_name Идентификатор имени модуля
         * @return Указатель на выделенную память или nullptr при ошибке
         *
         * Размер автоматически выравнивается до 4 байт. Аллокация добавляется в вектор отслеживания.
         */
        static void* allocate(u32 size, Module* owner_module, StringId module_name);

        /**
         * @brief Преобразование указателя в 32-битное смещение
         * @param ptr Указатель для преобразования
         * @return Смещение от начала пула или 0 при ошибке
         */
        static u32 pointer_to_offset(const void* ptr) {
            if (!ptr || !pool_base) return 0;
            u8* byte_ptr = static_cast<u8*>(const_cast<void*>(ptr));
            u64 offset = byte_ptr - pool_base;
            return (offset <= 0xFFFFFFFF) ? static_cast<u32>(offset) : 0;
        }

        /**
         * @brief Преобразование 32-битного смещения в указатель
         * @param offset Смещение от начала пула
         * @return Указатель на память или nullptr при недопустимом смещении
         */
        static void* offset_to_pointer(u32 offset) {
            if (offset == 0 || !pool_base || offset >= pool_size) return nullptr;
            return pool_base + offset;
        }

        /**
         * @brief Освобождение всех аллокаций модуля
         * @param module_name Идентификатор имени модуля для освобождения
         * @return true если память была освобождена, false если модуль не найден
         *
         * Удаляет все аллокации указанного модуля из вектора отслеживания.
         * Фактическое освобождение памяти происходит при компактификации.
         */
        static bool deallocate(StringId module_name);

        /**
         * @brief Компактификация пула памяти
         * @return true если компактификация выполнена, false при ошибке
         *
         * Перераспределяет активные аллокации для устранения фрагментации
         * и освобождения непрерывного блока памяти в конце пула.
         */
        static bool compactify();

        /**
         * @brief Поиск аллокации по имени модуля
         * @param module_name Идентификатор имени модуля для поиска
         * @return Указатель на первую найденную аллокацию или nullptr
         */
        static void* find_allocation(StringId module_name) {
            auto it = std::find_if(allocations.begin(), allocations.end(),
                [module_name](const Allocation& alloc) {
                    return alloc.module_name == module_name;
                });
            return it != allocations.end() ? it->address : nullptr;
        }

        // === СТАТИСТИКА И СЛУЖЕБНЫЕ МЕТОДЫ ===

        /// @brief Проверка инициализации пула
        static bool is_initialized() { return pool_base != nullptr; }

        /// @brief Получение размера пула (без учета INTERNAL_OFFSET)
        static u32 get_pool_size() { return pool_size - INTERNAL_OFFSET; }

        /// @brief Получение объема использованной памяти
        static u32 get_used_memory() {
            return current_offset > INTERNAL_OFFSET ? current_offset - INTERNAL_OFFSET : 0;
        }

        /// @brief Получение объема свободной памяти
        static u32 get_free_memory() {
            return get_pool_size() - get_used_memory();
        }

        /// @brief Получение количества активных аллокаций
        static u32 get_allocation_count() { return static_cast<u32>(allocations.size()); }

        /// @brief Получение базового адреса пула (без учета INTERNAL_OFFSET)
        static void* get_base_address() { return pool_base; }

        /**
         * @brief Расчет коэффициента использования пула
         * @return Процент использования памяти от 0.0 до 100.0
         */
        static double get_utilization() {
            u32 user_pool_size = get_pool_size();
            return user_pool_size > 0 ? (static_cast<double>(get_used_memory()) / user_pool_size) * 100.0 : 0.0;
        }

        /**
         * @brief Выравнивание размера до границы 4 байт
         * @param n Исходный размер
         * @return Выровненный размер
         */
        static constexpr u32 align_size(u32 n) { return (n + 3) & ~3; }

        /**
         * @brief Освобождение ресурсов пула
         *
         * Освобождает всю память пула и сбрасывает внутреннее состояние.
         */
        static void shutdown();

        static std::string inspect() {
            std::string result = "BinaryFilePool:\n";
            result += std::format("  pool_base: {}\n", (u32)pool_base);
            result += std::format("  pool_size: {}\n", pool_size);
            result += std::format("  current_offset: {}\n", current_offset);
            result += std::format("  allocations: {}\n", allocations.size());

            for (size_t i = 0; i < allocations.size(); i++) {
                const auto& alloc = allocations[i];
                result += std::format("    [{}] module: {}, addr: {}, size: {}, owner: {}\n",
                    i, alloc.module_name, (u32)alloc.address, alloc.size, (u32)alloc.owner_module);
            }

            result += std::format("  INTERNAL_OFFSET: {}\n", INTERNAL_OFFSET);
            result += std::format("  g_module_pool_base: {}\n", (u32)g_module_pool_base);

            return result;
        }

        static std::string stats() {
            return std::format("Pool: size={}, used={}, free={}, allocs={}",
                get_pool_size(), get_used_memory(), get_free_memory(), allocations.size());
        }
    };

} // namespace vm