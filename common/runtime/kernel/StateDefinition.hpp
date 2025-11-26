#pragma once

#include "common/runtime/ForwardDeclarations.hpp"
#include "common/runtime/lib/StringId.hpp"
#include "common/runtime/lib/Types.hpp"
#include "common/runtime/files/BinaryFile.hpp"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include <unordered_map>
#include <string>

namespace runtime
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
        StateDefinition(StringId state_name, ByteCode* update_code = nullptr)
            : name(state_name), update_bytecode(update_code) {
        }

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
        bool has_event(StringId event_type) const {
            return event_handlers.find(event_type) != event_handlers.end();
        }

        /// Проверяет, есть ли хотя бы один обработчик событий
        bool has_any_events() const {
            return !event_handlers.empty();
        }

        // ===== МЕТОДЫ ДЛЯ РАБОТЫ С СОБЫТИЯМИ =====

        /// Добавляет обработчик события
        /// @param event_type - тип события
        /// @param handler - байткод обработчика
        void add_event_handler(StringId event_type, ByteCode* handler) {
            event_handlers[event_type] = handler;
        }

        /// Удаляет обработчик события
        /// @param event_type - тип события для удаления
        void remove_event_handler(StringId event_type) {
            event_handlers.erase(event_type);
        }

        /// Получает обработчик события
        /// @param event_type - тип события
        /// @return указатель на байткод или nullptr если не найден
        ByteCode* get_event_handler(StringId event_type) const {
            auto it = event_handlers.find(event_type);
            return it != event_handlers.end() ? it->second : nullptr;
        }

        // ===== МЕТОДЫ ДЛЯ РАБОТЫ С НАСЛЕДОВАНИЕМ =====

        /// Проверяет, наследуется ли текущее состояние от указанного
        /// @param other - проверяемое родительское состояние
        /// @return true если это состояние наследуется от other
        bool inherits_from(StringId other) const {
            return parent_state == other;
        }

        /// Проверяет, может ли состояние быть активировано напрямую
        /// Виртуальные состояния могут использоваться только как базовые для наследования
        bool can_activate_directly() const {
            return !metadata.is_virtual;
        }

        // ===== МЕТОДЫ ДЛЯ СБОРА СТАТИСТИКИ =====

        /// Обновляет статистику времени пребывания в состоянии
        /// @param delta_time - время в миллисекундах с последнего обновления
        void update_time_stats(u32 delta_time) {
            stats.time_in_state += delta_time;
        }

        /// Записывает факт входа в состояние
        void record_enter() {
            stats.enter_count++;
        }

        /// Записывает факт выполнения обновления состояния
        void record_update() {
            stats.update_count++;
        }

        /// Записывает факт обработки события
        void record_event_handled() {
            stats.events_handled++;
        }

        /// Добавляет такты процессора к общей статистике
        /// @param cycles - количество тактов для добавления
        void record_cycles(u64 cycles) {
            stats.total_cycles += cycles;
        }

        /// Обновляет статистику последовательных выполнений
        /// @param consecutive_frames - количество последовательных кадров
        void update_consecutive_frames(u32 consecutive_frames) {
            if (consecutive_frames > stats.max_consecutive_frames) {
                stats.max_consecutive_frames = consecutive_frames;
            }
        }

        // ===== СЛУЖЕБНЫЕ МЕТОДЫ =====

        /// Сбрасывает всю статистику выполнения
        void reset_statistics() {
            stats = Statistics{};
        }

        /// Проверяет, является ли состояние валидным (имеет хотя бы один обработчик)
        bool is_valid() const {
            return has_enter() || has_exit() || has_update() ||
                has_trans() || has_post() || has_any_events();
        }

        /// Получает имя состояния в виде строки (для отладки)
        std::string get_name_string() const {
            return lib::to_string(name);
        }

        /// Получает имя родительского состояния в виде строки (для отладки)
        std::string get_parent_name_string() const {
            return lib::to_string(parent_state);
        }

        /// Форматирует состояние в читаемую строку (для отладки)
        std::string to_string() const {
            std::string result = fmt::format("State('{}'", get_name_string());

            if (parent_state != string_id::NONE) {
                result += fmt::format(", parent:'{}'", get_parent_name_string());
            }

            if (metadata.is_virtual) {
                result += ", virtual";
                if (metadata.is_override) {
                    result += "-override";
                }
            }

            // Собираем информацию об обработчиках
            std::vector<std::string> handlers;
            if (has_enter()) handlers.push_back("enter");
            if (has_trans()) handlers.push_back("trans");
            if (has_update()) handlers.push_back("update");
            if (has_post()) handlers.push_back("post");
            if (has_exit()) handlers.push_back("exit");
            if (has_any_events()) {
                handlers.push_back(fmt::format("events:{}", event_handlers.size()));
            }

            if (!handlers.empty()) {
                result += fmt::format(", handlers:[{}]", fmt::join(handlers, ", "));
            }

            result += ")";
            return result;
        }

        /// Форматирует статистику в читаемую строку
        std::string statistics_to_string() const {
            return fmt::format(
                "State '{}' stats: time={}ms, enters={}, updates={}, events={}, cycles={}",
                get_name_string(),
                stats.time_in_state,
                stats.enter_count,
                stats.update_count,
                stats.events_handled,
                stats.total_cycles
            );
        }

        // ===== ОПЕРАТОРЫ СРАВНЕНИЯ =====

        /// Сравнение по имени состояния
        bool operator==(const StateDefinition& other) const {
            return name == other.name;
        }

        bool operator!=(const StateDefinition& other) const {
            return !(*this == other);
        }

        /// Сравнение для упорядочивания (для использования в ordered контейнерах)
        bool operator<(const StateDefinition& other) const {
            return name < other.name;
        }
    };

} // namespace runtime

// ===== ПОДДЕРЖКА ДЛЯ STD::UNORDERED_CONTAINERS =====
namespace std {
    template<>
    struct hash<runtime::StateDefinition> {
        size_t operator()(const runtime::StateDefinition& state) const {
            return hash<StringId>{}(state.name);
        }
    };
}