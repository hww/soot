#pragma once

#include "common/runtime/ForwardDeclarations.hpp"
#include "common/runtime/lib/StringId.hpp"
#include "common/runtime/lib/Types.hpp"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include <unordered_map>
#include <string>

using namespace runtime::lib;
using namespace runtime::files;

namespace runtime::kernel
{
    /**
     * StateDefinition - определение состояния для конечного автомата (State Machine)
     * Соответствует GOAL state с дополнительной функциональностью для управления поведением
     */
    struct StateDefinition {
        // ===== ОСНОВНЫЕ ИДЕНТИФИКАТОРЫ =====

        /// Уникальное имя состояния
        StringId name;

        /// Родительское состояние для наследования (StringId::NONE если нет наследования)
        StringId parent_state = SID("null");

        // ===== ОБРАБОТЧИКИ ПОВЕДЕНИЯ (указатели на байткод) =====

        /// Байткод для входа в состояние (соответствует enter в GOAL)
        /// Выполняется при переходе в это состояние
        ByteCode* enter_bytecode = nullptr;

        /// Байткод для обработки перехода между состояниями (соответствует trans в GOAL)
        /// Обрабатывает условия перехода до выполнения основного кода состояния
        ByteCode* trans_bytecode = nullptr;

        /// Основной код состояния (соответствует code в GOAL)
        /// Выполняется при каждом обновлении, может быть прерван suspend()
        ByteCode* update_bytecode = nullptr;

        /// Байткод для пост-обработки состояния (соответствует post в GOAL)
        /// Выполняется после основного кода, используется для очистки, проверок, пост-обработки
        ByteCode* post_bytecode = nullptr;

        /// Байткод для выхода из состояния (соответствует exit в GOAL)
        /// Выполняется при выходе из состояния, используется для освобождения ресурсов
        ByteCode* exit_bytecode = nullptr;

        // ===== ОБРАБОТЧИКИ СОБЫТИЙ =====

        /// Таблица обработчиков событий (event_type -> handler)
        /// В GOAL соответствует event_bytecode, но здесь организована в виде таблицы
        std::unordered_map<StringId, ByteCode*> event_handlers;

        // ===== МЕТАДАННЫЕ ДЛЯ УПРАВЛЕНИЯ НАСЛЕДОВАНИЕМ И ПОВЕДЕНИЕМ =====

        struct Metadata {
            /// Виртуальное состояние - не может быть активировано напрямую,
            /// используется только как базовое для наследования
            bool is_virtual = false;

            /// Переопределяющее состояние (override в GOAL)
            bool is_override = false;

            /// Исходное местоположение в коде (для отладки)
            std::string source_location;

            /// Порядок определения (для разрешения конфликтов наследования)
            u32 definition_order = 0;

            /// Флаг, указывающий что состояние определено пользователем,
            /// а не сгенерировано автоматически
            bool user_defined = true;
        } metadata;

        // ===== СТАТИСТИКА ВЫПОЛНЕНИЯ =====

        struct Statistics {
            /// Общее время в состоянии (в миллисекундах)
            u32 time_in_state = 0;

            /// Количество входов в состояние
            u32 enter_count = 0;

            /// Количество выполненных обновлений состояния
            u32 update_count = 0;

            /// Количество обработанных событий
            u32 events_handled = 0;

            /// Общее количество тактов процессора, затраченных на выполнение состояния
            u64 total_cycles = 0;

            /// Максимальное количество последовательных кадров выполнения
            /// (полезно для обнаружения зацикливания состояния)
            u32 max_consecutive_frames = 0;

            /// Время последнего выполнения в микросекундах (для профилирования)
            u32 last_execution_time_us = 0;
        } stats;

        // ===== КОНСТРУКТОРЫ И ОПЕРАТОРЫ =====

        /// Основной конструктор
        /// @param state_name - идентификатор состояния
        /// @param update_code - основной код состояния (может быть nullptr)
        StateDefinition(StringId state_name, ByteCode* update_code = nullptr);

        /// Запрет копирования (указатели на байткод не должны копироваться)
        StateDefinition(const StateDefinition&) = delete;
        StateDefinition& operator=(const StateDefinition&) = delete;

        /// Разрешение перемещения
        StateDefinition(StateDefinition&& other) noexcept = default;
        StateDefinition& operator=(StateDefinition&& other) noexcept = default;

        // ===== МЕТОДЫ ПРОВЕРКИ НАЛИЧИЯ ОБРАБОТЧИКОВ =====

        /// Проверяет наличие обработчика входа в состояние
        bool has_enter() const { return enter_bytecode != nullptr; }

        /// Проверяет наличие обработчика выхода из состояния
        bool has_exit() const { return exit_bytecode != nullptr; }

        /// Проверяет наличие основного кода обновления
        bool has_update() const { return update_bytecode != nullptr; }

        /// Проверяет наличие Trans-обработчика (переход между состояниями)
        bool has_trans() const { return trans_bytecode != nullptr; }

        /// Проверяет наличие Post-обработчика (пост-обработка состояния)
        bool has_post() const { return post_bytecode != nullptr; }

        /// Проверяет наличие обработчика для конкретного типа события
        /// @param event_type - тип события
        bool has_event(StringId event_type) const;

        /// Проверяет, есть ли хотя бы один обработчик событий
        bool has_any_events() const;

        // ===== МЕТОДЫ ДЛЯ РАБОТЫ С СОБЫТИЯМИ =====

        /// Добавляет обработчик события
        /// @param event_type - тип события
        /// @param handler - байткод обработчика
        void add_event_handler(StringId event_type, ByteCode* handler);

        /// Удаляет обработчик события
        /// @param event_type - тип события для удаления
        void remove_event_handler(StringId event_type);

        /// Получает обработчик события
        /// @param event_type - тип события
        /// @return указатель на байткод или nullptr если не найден
        ByteCode* get_event_handler(StringId event_type) const;

        // ===== МЕТОДЫ ДЛЯ РАБОТЫ С НАСЛЕДОВАНИЕМ =====

        /// Проверяет, наследуется ли текущее состояние от указанного
        /// @param other - проверяемое родительское состояние
        /// @return true если это состояние наследуется от other
        bool inherits_from(StringId other) const;

        /// Проверяет, может ли состояние быть активировано напрямую
        /// Виртуальные состояния могут использоваться только как базовые для наследования
        bool can_activate_directly() const;

        // ===== МЕТОДЫ ДЛЯ СБОРА СТАТИСТИКИ =====

        /// Обновляет статистику времени пребывания в состоянии
        /// @param delta_time - время в миллисекундах с последнего обновления
        void update_time_stats(u32 delta_time);

        /// Записывает факт входа в состояние
        void record_enter();

        /// Записывает факт выполнения обновления состояния
        void record_update();

        /// Записывает факт обработки события
        void record_event_handled();

        /// Добавляет такты процессора к общей статистике
        /// @param cycles - количество тактов для добавления
        void record_cycles(u64 cycles);

        /// Обновляет статистику последовательных выполнений
        /// @param consecutive_frames - количество последовательных кадров
        void update_consecutive_frames(u32 consecutive_frames);

        // ===== СЛУЖЕБНЫЕ МЕТОДЫ =====

        /// Сбрасывает всю статистику выполнения
        void reset_statistics();

        /// Проверяет, является ли состояние валидным (имеет хотя бы один обработчик)
        bool is_valid() const;

        /// Получает имя состояния в виде строки (для отладки)
        std::string get_name_string() const;

        /// Получает имя родительского состояния в виде строки (для отладки)
        std::string get_parent_name_string() const;

        /// Форматирует состояние в читаемую строку (для отладки)
        std::string to_string() const;

        /// Форматирует статистику в читаемую строку
        std::string statistics_to_string() const;

        // ===== ОПЕРАТОРЫ СРАВНЕНИЯ =====

        /// Сравнение по имени состояния
        bool operator==(const StateDefinition& other) const;

        bool operator!=(const StateDefinition& other) const;

        /// Сравнение для упорядочивания (для использования в ordered контейнерах)
        bool operator<(const StateDefinition& other) const;
    };

} // namespace runtime

// ===== ПОДДЕРЖКА ДЛЯ STD::UNORDERED_CONTAINERS =====
namespace std {
    template<>
    struct hash<runtime::kernel::StateDefinition> {
        size_t operator()(const runtime::kernel::StateDefinition& state) const {
            return hash<StringId>{}(state.name);
        }
    };
}