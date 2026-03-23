#include "common/carbon/kernel/Process.hpp"
#include "common/carbon/kernel/DeadPool.hpp"
#include "common/carbon/kernel/EntityActor.hpp"
#include "common/carbon/kernel/StateDefinition.hpp"
#include "common/carbon/kernel/StateFrame.hpp"
#include "common/carbon/vm/VirtualMachine.hpp"
#include "common/util/Assert.hpp"
#include "common/util/Log.hpp"
#include <format>
#include <memory>

namespace runtime::kernel {

    // ============================================================================
    // Static Member Initialization
    // ============================================================================

    u32 Process::next_pid_ = INVALID_PROCESS_ID + 1; // Начинаем после корневого процесса

    // ============================================================================
    // Временные заглушки для отсутствующих компонентов
    // ============================================================================

    class StateRegistry {
    public:
        static StateDefinition* find_state(StringId state) {
            // TODO: Реализовать настоящий поиск состояний
            return nullptr;
        }
    };

    namespace vm {
        class VirtualMachine {
        public:
            static bool execute_frame(StackFrame* frame, Process* process) {
                // TODO: Реализовать выполнение фрейма
                return true;
            }

            static Variant execute_FunctionDesc(FunctionDesc* FunctionDesc) {
                // TODO: Реализовать выполнение байткода
                return Variant();
            }
        };
    }

    // ============================================================================
    // Constructor & Destructor
    // ============================================================================

    Process::Process(StringId name)
        : pid(generate_pid()), name(name), ConnectionList(this) {
        lg::debug("Process created: pid={}, name='{}'", pid, get_name_string());
    }

    Process::~Process() {
        lg::debug("Process destroyed: pid={}, name='{}'", pid, get_name_string());

        // Очищаем все соединения
        disconnect_all();

        // Очищаем стек фреймов
        while (stack_frame_top) {
            pop_frame();
        }

        // Очищаем потоки
        if (main_thread) {
            delete main_thread;
        }

        // Если есть top_thread и он отличается от main_thread
        if (top_thread && top_thread != main_thread) {
            delete top_thread;
        }

        // Освобождаем память кучи
        if (heap_base) {
            free(heap_base);
            heap_base = nullptr;
            heap_cur = nullptr;
            heap_top = nullptr;
        }
    }

    // ============================================================================
    // PID Generation
    // ============================================================================

    u32 Process::generate_pid() {
        // Ищем свободный PID, пропуская невалидные значения
        u32 candidate = next_pid_++;

        // Пропускаем INVALID_PROCESS_ID
        if (candidate == INVALID_PROCESS_ID) {
            candidate = next_pid_++;
        }

        // TODO: В реальной системе здесь должна быть проверка на коллизии
        // если процессы создаются в разных потоках

        return candidate;
    }

    // ============================================================================
    // State Management
    // ============================================================================

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

    bool Process::send_event(StringId event, u32 argc, Variant* argv) {
        if (!event_hook || !is_runnable()) {
            return false;
        }

        // Временная заглушка - TODO: реализовать полноценную обработку событий
        lg::debug("Process '{}': received event '{}' (args: {})",
            get_name_string(), lib::to_string(event), argc);

        return false;
    }

    // ============================================================================
    // Stack Operations
    // ============================================================================

    void Process::push_frame(StackFrame* frame) {
        ASSERT_MSG(frame != nullptr, "Cannot push null frame");

        frame->parent_ptr = stack_frame_top;
        stack_frame_top = frame;

        lg::debug("Process '{}': pushed frame '{}'",
            get_name_string(), lib::to_string(frame->name));
    }

    StackFrame* Process::pop_frame() {
        if (!stack_frame_top) {
            return nullptr;
        }

        StackFrame* frame = stack_frame_top;
        stack_frame_top = frame->parent_ptr;

        // Вызываем exit обработчик если есть
        frame->exit();

        lg::debug("Process '{}': popped frame '{}'",
            get_name_string(), lib::to_string(frame->name));

        delete frame;
        return stack_frame_top;
    }

    StackFrame* Process::find_frame(StringId frame_name) {
        StackFrame* current = stack_frame_top;
        while (current) {
            if (current->name == frame_name) {
                return current;
            }
            current = current->parent_ptr;
        }
        return nullptr;
    }

    // ============================================================================
    // Memory Management
    // ============================================================================

    void* Process::heap_alloc(u32 size) {
        if (!heap_base || !heap_cur) {
            lg::error("Process '{}': heap not initialized", get_name_string());
            return nullptr;
        }

        // Выравниваем размер
        size = (size + 15) & ~15; // 16-byte alignment

        // Проверяем достаточно ли памяти
        uintptr_t new_cur = reinterpret_cast<uintptr_t>(heap_cur) + size;
        uintptr_t heap_end = reinterpret_cast<uintptr_t>(heap_top);

        if (new_cur > heap_end) {
            lg::error("Process '{}': heap overflow (requested: {}, available: {})",
                get_name_string(), size, heap_end - reinterpret_cast<uintptr_t>(heap_cur));
            return nullptr;
        }

        void* allocated = heap_cur;
        heap_cur = reinterpret_cast<void*>(new_cur);

        lg::debug("Process '{}': allocated {} bytes at {:x}",
            get_name_string(), size, reinterpret_cast<uintptr_t>(allocated));

        return allocated;
    }

    void Process::heap_free(void* ptr) {
        // В простейшей реализации не делаем ничего
        // В более сложной - можем реализовать настоящий аллокатор
        lg::debug("Process '{}': freed memory at {:x}",
            get_name_string(), reinterpret_cast<uintptr_t>(ptr));
    }

    // ============================================================================
    // Execution Control
    // ============================================================================

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
            bool completed = vm::VirtualMachine::execute_frame(main_thread, this);

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

    void Process::suspend() {
        if (status == ProcessStatus::RUNNING) {
            status = ProcessStatus::SUSPENDED;
            lg::debug("Process '{}': suspended", get_name_string());
        }
    }

    void Process::resume() {
        if (status == ProcessStatus::SUSPENDED) {
            status = ProcessStatus::READY;
            lg::debug("Process '{}': resumed", get_name_string());
        }
    }

    void Process::activate(void* stack_top) {
        // Инициализируем кучу если нужно
        if (!heap_base && allocated_length > 0) {
            heap_base = malloc(allocated_length);
            if (!heap_base) {
                lg::error("Process '{}': failed to allocate heap memory", get_name_string());
                return;
            }
            heap_cur = heap_base;
            heap_top = reinterpret_cast<void*>(
                reinterpret_cast<uintptr_t>(heap_base) + allocated_length
                );
        }

        // Создаем главный поток если нужно
        if (!main_thread && current_state && current_state->update_FunctionDesc) {
            // main_thread = new StackFrame(current_state->update_FunctionDesc, nullptr, SID("main_thread"));
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

    // ============================================================================
    // Connection Management
    // ============================================================================

    void Process::connect_to(Process* other, StringId connection_type) {
        if (!other) return;

        // TODO: Реализовать логику соединений
        lg::debug("Process '{}': connected to '{}' with type '{}'",
            get_name_string(),
            other->get_name_string(),
            lib::to_string(connection_type));
    }

    void Process::disconnect_from(Process* other) {
        if (!other) return;

        // TODO: Реализовать логику разъединений
        lg::debug("Process '{}': disconnected from '{}'",
            get_name_string(), other->get_name_string());
    }

    void Process::disconnect_all() {
        // TODO: Реализовать очистку всех соединений
        lg::debug("Process '{}': disconnected from all", get_name_string());
    }

    // ============================================================================
    // Tree Operations
    // ============================================================================

    void Process::add_child(Process* child) {
        if (!child) return;

        child->parent = this;
        child->brother = this->child;
        this->child = child;

        lg::debug("Process '{}': added child '{}'",
            get_name_string(), child->get_name_string());
    }

    void Process::remove_child(Process* child) {
        if (!child) return;

        if (this->child == child) {
            this->child = child->brother;
        }
        else {
            // Ищем ребенка в списке
            Process* current = this->child;
            Process* prev = nullptr;

            while (current && current != child) {
                prev = current;
                current = current->brother;
            }

            if (current == child && prev) {
                prev->brother = child->brother;
            }
        }

        child->parent = nullptr;
        child->brother = nullptr;

        lg::debug("Process '{}': removed child '{}'",
            get_name_string(), child->get_name_string());
    }

    Process* Process::find_child(StringId process_name) {
        Process* current = child;
        while (current) {
            if (current->name == process_name) {
                return current;
            }
            current = current->brother;
        }
        return nullptr;
    }

    void Process::for_each_child(std::function<void(Process*)> func) {
        Process* current = child;
        while (current) {
            func(current);
            current = current->brother;
        }
    }

    // ============================================================================
    // Utility Methods
    // ============================================================================

    std::string Process::to_string() const {
        return std::format("Process(pid:{}, name:'{}', status:{}, state:'{}')",
            pid,
            get_name_string(),
            static_cast<int>(status),
            current_state ? current_state->get_name_string() : "null"
        );
    }

    void Process::dump_info() const {
        lg::debug("=== Process Info ===");
        lg::debug("  PID: {}", pid);
        lg::debug("  Name: '{}'", get_name_string());
        lg::debug("  Status: {}", static_cast<int>(status));
        lg::debug("  Mask: 0x{:08x}", static_cast<u32>(mask));
        lg::debug("  State: '{}'", current_state ? current_state->get_name_string() : "null");
        lg::debug("  Next State: '{}'", next_state ? next_state->get_name_string() : "null");
        lg::debug("  Heap: {}/{} bytes", heap_used(), heap_size());
        lg::debug("  Stack Depth: {}", [this]() {
            u32 depth = 0;
            StackFrame* frame = stack_frame_top;
            while (frame) { depth++; frame = frame->parent_ptr; }
            return depth;
            }());
        lg::debug("  Children: {}", [this]() {
            u32 count = 0;
            Process* child = this->child;
            while (child) { count++; child = child->brother; }
            return count;
            }());
    }

    // ============================================================================
    // Private Methods
    // ============================================================================

    bool Process::execute_immediate_transition(StateDefinition* new_state) {
        // TODO: Реализовать немедленный переход
        // (когда процесс выполняется в текущем потоке)
        lg::debug("Process '{}': immediate transition to '{}'",
            get_name_string(), new_state->get_name_string());
        return true;
    }

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
        if (main_thread && new_state->update_FunctionDesc) {
            // TODO: Обновить байткод главного потока
        }

        lg::debug("Process '{}': transitioned from '{}' to '{}'",
            get_name_string(),
            old_state ? old_state->get_name_string() : "null",
            new_state->get_name_string());

        return true;
    }

    void Process::update_state_hooks() {
        if (current_state) {
            trans_hook = current_state->trans_FunctionDesc;
            post_hook = current_state->post_FunctionDesc;
            // event_hook = current_state->event_FunctionDesc; // TODO: исправить когда будет event_FunctionDesc
        }
        else {
            trans_hook = nullptr;
            post_hook = nullptr;
            event_hook = nullptr;
        }
    }

    void Process::execute_trans_handler() {
        if (trans_hook) {
            // StackFrame* trans_frame = new StackFrame(trans_hook, stack_frame_top, SID("trans"));
            StackFrame* saved_top_thread = top_thread;

            // top_thread = trans_frame;
            // VirtualMachine::execute_frame(trans_frame, this);
            vm::VirtualMachine::execute_FunctionDesc(trans_hook); // Временная реализация

            top_thread = saved_top_thread;

            // delete trans_frame;
        }
    }

    void Process::execute_post_handler() {
        if (post_hook) {
            // StackFrame* post_frame = new StackFrame(post_hook, stack_frame_top, SID("post"));
            StackFrame* saved_top_thread = top_thread;

            // top_thread = post_frame;
            // VirtualMachine::execute_frame(post_frame, this);
            vm::VirtualMachine::execute_FunctionDesc(post_hook); // Временная реализация

            top_thread = saved_top_thread;

            // delete post_frame;
        }
    }

    void Process::cleanup() {
        // Очищаем состояние
        current_state = nullptr;
        next_state = nullptr;

        // Очищаем обработчики
        trans_hook = nullptr;
        post_hook = nullptr;
        event_hook = nullptr;

        // Сбрасываем статус
        status = ProcessStatus::DEAD;

        // Очищаем маски (кроме PROCESS_TREE если это узел)
        if (!has_mask(ProcessMask::PROCESS_TREE)) {
            mask = ProcessMask::NONE;
        }

        lg::debug("Process '{}': cleaned up", get_name_string());
    }

} // namespace runtime::kernel