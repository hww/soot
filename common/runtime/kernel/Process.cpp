#include "common/runtime/kernel/Process.hpp"
#include "common/runtime/kernel/DeadPool.hpp"
#include "common/runtime/kernel/EntityActor.hpp"
#include "common/runtime/kernel/StateDefinition.hpp" // Добавляем этот include
#include "common/runtime/kernel/StateFrame.hpp" // Добавляем этот include
#include "common/runtime/vm/VirtualMachine.hpp"
#include "common/util/Assert.hpp"
#include "common/util/Log.hpp"
#include <format>
#include <memory>

namespace runtime::kernel {

    // Временная заглушка для StateRegistry
    class StateRegistry {
    public:
        static StateDefinition* find_state(StringId state) {
            // TODO: Реализовать настоящий поиск состояний
            return nullptr;
        }
    };

    // Временная заглушка для VirtualMachine::execute_frame
    namespace vm {
        class VirtualMachine {
        public:
            static bool execute_frame(StackFrame* frame, Process* process) {
                // TODO: Реализовать выполнение фрейма
                return true;
            }
        };
    }

    Process::Process(u32 pid, StringId name)
        : pid(pid), name(name), ConnectionList(this) { // Инициализируем ConnectionList
        lg::debug("Process created: pid={}, name='{}'", pid, get_name_string());
    }

    // Исправляем метод go_state
    bool Process::go_state(StringId state) {
        // Находим определение состояния
        StateDefinition* new_state = StateRegistry::find_state(state);
        if (!new_state) {
            lg::error("Process '{}': state '{}' not found",
                get_name_string(), lib::to_string(state));
            return false;
        }

        // Проверяем возможность перехода
        if (new_state->metadata.is_virtual && !new_state->metadata.is_override) {
            lg::error("Process '{}': cannot activate virtual state '{}'",
                get_name_string(), new_state->get_name_string());
            return false;
        }

        // Устанавливаем следующее состояние
        next_state = new_state;
        add_mask(ProcessMask::GOING);

        lg::debug("Process '{}': scheduling transition to state '{}'",
            get_name_string(), new_state->get_name_string());

        return true;
    }

    // Исправляем метод send_event
    bool Process::send_event(StringId event, u32 argc, Variant* argv) {
        if (!event_hook || !is_runnable()) {
            return false;
        }

        // Создаем временный фрейм для обработки события
        // StackFrame* event_frame = new StackFrame(event_hook, stack_frame_top, SID("event"));
        // TODO: Исправить создание фрейма когда будет правильный конструктор

        // Передаем аргументы события
        // if (argc > 0 && argv) {
        //     for (u32 i = 0; i < argc && i < MAX_ARGS; i++) {
        //         event_frame->get_argument(i) = argv[i];
        //     }
        //     event_frame->argc = argc;
        // }

        // Сохраняем текущий поток
        StackFrame* saved_top_thread = top_thread;
        // top_thread = event_frame;

        // Выполняем обработчик события
        // bool result = VirtualMachine::execute_frame(event_frame, this);
        bool result = false; // Временная заглушка

        // Восстанавливаем поток
        top_thread = saved_top_thread;
        // delete event_frame;

        if (result) {
            lg::debug("Process '{}': handled event '{}'",
                get_name_string(), lib::to_string(event));
        }

        return result;
    }

    // Исправляем метод activate
    void Process::activate(void* stack_top) {
        // Инициализируем кучу если нужно
        if (!heap_base && allocated_length > 0) {
            heap_base = malloc(allocated_length);
            heap_cur = heap_base;
            heap_top = reinterpret_cast<void*>(
                reinterpret_cast<uintptr_t>(heap_base) + allocated_length
                );
        }

        // Создаем главный поток если нужно
        if (!main_thread && current_state && current_state->update_bytecode) {
            // main_thread = new StackFrame(current_state->update_bytecode, nullptr, SID("main_thread"));
            // main_thread->frame_type = StackFrame::FrameType::GENERIC;
            // TODO: Исправить создание фрейма
        }

        top_thread = main_thread;
        status = ProcessStatus::READY;

        // Создаем StateFrame для текущего состояния
        if (current_state && !current_state_frame) {
            // current_state_frame = new StateFrame(current_state, this, stack_frame_top);
            // push_frame(current_state_frame);

            // Выполняем enter-обработчик
            // current_state_frame->execute_enter();
            // TODO: Исправить когда будет StateFrame
        }

        // Обновляем обработчики
        update_state_hooks();

        lg::debug("Process '{}': activated with state '{}'",
            get_name_string(),
            current_state ? current_state->get_name_string() : "null");
    }

    // Исправляем метод execute_quantum
    bool Process::execute_quantum() {
        // Проверяем можно ли выполнять процесс
        if (!is_runnable()) {
            return false;
        }

        // Проверяем отложенные переходы между состояниями
        if (has_pending_transition()) {
            if (!execute_deferred_transition(next_state)) {
                lg::error("Process '{}': failed to execute deferred transition", get_name_string());
                return false;
            }
            next_state = nullptr;
            remove_mask(ProcessMask::GOING);
        }

        // Если процесс спит (только SLEEP_CODE), выполняем только trans/post
        if (has_mask(ProcessMask::SLEEP_CODE)) {
            execute_trans_handler();
            execute_post_handler();
            return true;
        }

        // Полное выполнение: trans -> main code -> post
        status = ProcessStatus::RUNNING;

        // 1. Выполняем trans-обработчик
        execute_trans_handler();

        // 2. Выполняем основной код
        if (main_thread && main_thread->byte_code) {
            top_thread = main_thread;
            // bool completed = VirtualMachine::execute_frame(main_thread, this);
            bool completed = true; // Временная заглушка

            if (!completed) {
                // Код приостановлен (suspend)
                status = ProcessStatus::SUSPENDED;
            }
        }

        // 3. Выполняем post-обработчик
        execute_post_handler();

        if (status == ProcessStatus::RUNNING) {
            status = ProcessStatus::SUSPENDED; // Готов к следующему кадру
        }

        return true;
    }

    // Исправляем метод execute_deferred_transition
    bool Process::execute_deferred_transition(StateDefinition* new_state) {
        // Удаляем текущий StateFrame (вызовет exit-обработчик)
        if (current_state_frame) {
            // Ищем StateFrame в стеке
            StackFrame* frame = stack_frame_top;
            while (frame && frame != current_state_frame) {
                frame = frame->parent_ptr;
            }

            if (frame == current_state_frame) {
                pop_frame(); // Вызовет exit-обработчик
            }

            current_state_frame = nullptr;
        }

        // Обновляем текущее состояние
        StateDefinition* old_state = current_state;
        current_state = new_state;

        // Создаем новый StateFrame
        // current_state_frame = new StateFrame(new_state, this, stack_frame_top);
        // push_frame(current_state_frame);

        // Выполняем enter-обработчик нового состояния
        // current_state_frame->execute_enter();

        // Обновляем обработчики
        update_state_hooks();

        // Обновляем главный поток если нужно
        if (main_thread && new_state->update_bytecode) {
            // TODO: Обновить байткод главного потока
        }

        lg::debug("Process '{}': transitioned from '{}' to '{}'",
            get_name_string(),
            old_state ? old_state->get_name_string() : "null",
            new_state->get_name_string());

        return true;
    }

    // Остальные методы Process.cpp остаются без изменений...

} // namespace runtime::kernel