#include "common/carbon/kernel/Process.hpp"
#include "common/carbon/kernel/StateFrame.hpp"
#include "common/carbon/vm/VirtualMachine.hpp"
#include "common/util/Assert.hpp"
#include "common/util/Log.hpp"
#include "files/StateDesc.hpp"
#include "kernel/EventMessage.hpp"
#include "kernel/Kernel.hpp"
#include "lib/StringId.hpp"
#include "lib/Variant.hpp"
#include "vm/StackFrame.hpp"
#include <format>
#include <memory>

namespace carbon::kernel {

    // ============================================================================
    // Static Member Initialization
    // ============================================================================

    u32 Process::next_pid_ = INVALID_PROCESS_ID + 1; // Начинаем после корневого процесса

    // ============================================================================
    // Constructor & Destructor
    // ============================================================================

    Process::Process() 
        : pid(generate_pid())
        , status(ProcessStatus::DEAD)
        , mask(ProcessMask::NONE)
        , type(nullptr)
        , pool(nullptr)
        , heap_base(nullptr)
        , heap_top(nullptr)
        , heap_cur(nullptr)
        , allocated_length(0)
        , main_thread(nullptr)
        , top_thread(nullptr)
        , stack_frame_top(nullptr)
        , current_state_frame(nullptr)
        , current_state(nullptr)
        , next_state(nullptr)
        , trans_hook(nullptr)
        , post_hook(nullptr)
        , event_handler(nullptr)
        , entity(nullptr) {
    }

    Process::~Process() {
        lg::debug("Process destroyed: pid={}, name='{}'", pid, get_name_string());

        disconnect_all();

        // Очищаем стек фреймов
        while (stack_frame_top) {
            pop_frame();
        }

        // Очищаем потоки
        if (main_thread) {
            main_thread = nullptr;
        }

        if (top_thread && top_thread != main_thread) {
            top_thread = nullptr;
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

    /**
     * Ищем свободный PID, пропуская невалидные значения
     */
    u32 Process::generate_pid() {
        
        u32 candidate = next_pid_++;
        if (candidate == INVALID_PROCESS_ID) {
            candidate = next_pid_++;
        }
        return candidate;
    }

    // ============================================================================
    // State Management
    // ============================================================================

    bool Process::go_state(StringId state) {
        // Находим определение состояния
        auto new_state = this->type->resolve_state(state);
        return go_state(new_state);
    }

    bool Process::go_state(StateDesc* new_state) {

        // Проверить что состояние валидное
        if (!new_state) {
            lg::error("Process '{}': state not found (nullptr)",
                get_name_string());
            return false;
        }
        
        // Проверяем возможность перехода
        if (new_state->is_virtual() && !new_state->is_override()) {
            lg::error("Process '{}': cannot activate virtual state '{}'",
                get_name_string(), new_state != nullptr ? new_state->name.to_string() : "null");
            return false;
        }

        auto vm = Kernel::instance().virtual_machine();

        // Если уже есть текущее состояние, нужно выйти из него
        if (current_state) {
            // Exit текущего состояния (через StateFrame)
            if (current_state_frame) {
                // Удаляем текущий StateFrame, что вызовет exit
                pop_frame();  // StateFrame вызовет exit при разрушении
                current_state_frame = nullptr;
            }
            
            // Очищаем хуки текущего состояния
            trans_hook = nullptr;
            post_hook = nullptr;
            event_handler = nullptr;
        }
        
        // Устанавливаем новое состояние
        current_state = new_state;
        
        // Копируем делегаты из StateDesc в процесс (как в GOAL)
        trans_hook = new_state->get_trans_function();
        post_hook = new_state->get_post_function();
        event_handler = new_state->get_event_handler();  // или event_handlers, если нужно
        
        // Создаём новый StateFrame (будет вызывать exit при выходе)
        current_state_frame = std::make_shared<StateFrame>(new_state, stack_frame_top);
        push_frame(current_state_frame);
        
        // Выполняем enter-обработчик нового состояния
        auto enter = new_state->get_enter_function();
        if (enter) {
            // Создаём временный фрейм для enter
            auto enter_frame = std::make_shared<StackFrame>(
                enter,
                nullptr,
                StackFrame::FrameType::GENERIC,
                SID("state_enter")
            );
            
            auto saved_top_thread = top_thread;
            top_thread = enter_frame;
            vm.execute(enter_frame);
            top_thread = saved_top_thread;
            
            enter_frame.reset();
        }
        
        lg::debug("Process '{}': transitioned to state '{}'",
            get_name_string(), new_state != nullptr ? new_state->name.to_string() : "null");
        
        return true;
    }

    // ============================================================================
    // update_state_hooks - Уже есть, но добавим проверку
    // ============================================================================

    void Process::update_state_hooks(VirtualMachine& vm) {
        (void)vm; // Подавляем warning о неиспользуемом параметре
        if (current_state) {
            trans_hook = current_state->get_trans_function();
            post_hook = current_state->get_post_function();
            event_handler = current_state->get_event_handler();
        }
    }

    // ============================================================================
    // Message Send Management
    // ============================================================================

    /**
     * Send message from this object to target
     */
    bool Process::send_event(Process* target, u32 argc, StringId event, Variant* argv) {
        if (!target || !target->has_event_handler() || !target->is_runnable()) {
            return false;
        }

        lg::debug("Process '{}': sending event '{}' to '{}' (args: {})",
            get_name_string(), event.to_string(), target->get_name_string(), argc);

        EventMessage message;
        message.from = this;
        message.to = target;
        message.message = event;
        message.num_params = argc;
        
        for (u32 i = 0; i < argc && i < EventMessage::MAX_PARAMS; i++) {
            message.params[i] = argv[i];
        }
        
        return target->execute_event(this, argc, event, &message);
    }

    /**
     * Send message from this object to target
     */
    bool Process::send_event(Process* target, u32 argc, StringId event, EventMessage* message) {
        if (!target || !target->has_event_handler() || !target->is_runnable()) {
            return false;
        }
        
        if (!message) {
            return send_event(target, argc, event, static_cast<Variant*>(nullptr));
        }
        
        message->from = this;
        message->to = target;
        message->message = event;
        message->num_params = argc;
        
        return target->execute_event(this, argc, event, message);
    }

    
    /**
     * Receive the event
     */
    bool Process::execute_event(Process* sender, u32 argc, StringId event, EventMessage* message) {
        if (!has_event_handler()) return false;

        auto saved_top_thread = top_thread;
        auto& vm = Kernel::instance().virtual_machine();

        auto event_frame = std::make_shared<StackFrame>(
            event_handler,
            nullptr,
            StackFrame::FrameType::GENERIC,
            event
        );

        // Исправлено: используем ссылки для изменения аргументов
        event_frame->set_argument(0, Variant(sender, TypeIds::process));
        event_frame->set_argument(1, Variant(argc));
        event_frame->set_argument(2, Variant(event));
        event_frame->set_argument(3, Variant(message, TypeIds::event_message));

        auto result = vm.execute(event_frame);
        
        top_thread = saved_top_thread;
        event_frame.reset();

        return result.to_bool();
    }

    // ============================================================================
    // Stack Operations
    // ============================================================================

    void Process::push_frame(std::shared_ptr<StackFrame> frame) {
        ASSERT_MSG(frame != nullptr, "Cannot push null frame");

        frame->parent = stack_frame_top;
        stack_frame_top = frame;

        // Если это StateFrame, обновляем current_state_frame
        if (frame->frame_type == StackFrame::FrameType::STATE) {
            current_state_frame = frame;
        }

        lg::debug("Process '{}': pushed frame '{}'", get_name_string(), frame->name);
    }

    std::shared_ptr<StackFrame> Process::pop_frame() {
        if (!stack_frame_top) {
            return nullptr;
        }

        std::shared_ptr<StackFrame> frame = stack_frame_top;
        stack_frame_top = frame->parent;

        // Если это StateFrame, очищаем current_state_frame
        if (frame == current_state_frame) {
            current_state_frame = nullptr;
        }

        // Вызываем exit обработчик если есть
        frame->exit();

        lg::debug("Process '{}': popped frame '{}'", get_name_string(), frame->name);

        return stack_frame_top;
    }

    std::shared_ptr<StackFrame> Process::find_frame(StringId frame_name) {
        std::shared_ptr<StackFrame> current = stack_frame_top;
        while (current) {
            if (current->name == frame_name) {
                return current;
            }
            current = current->parent;
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
    // Execution
    // ============================================================================

    bool Process::execute_quantum() {
        if (!is_runnable()) {
            return false;
        }

        auto& vm = Kernel::instance().virtual_machine();

        if (has_pending_transition()) {
            if (!execute_deferred_transition(next_state)) {
                lg::error("Process '{}': failed to execute deferred transition", get_name_string());
                return false;
            }
            next_state = nullptr;
            remove_mask(ProcessMask::GOING);
        }

        if (has_mask(ProcessMask::SLEEP_CODE)) {
            execute_trans_handler(vm);
            execute_post_handler(vm);
            return true;
        }

        status = ProcessStatus::RUNNING;

        // 1. Execute trans handler
        execute_trans_handler(vm);

        // 2. Execute main code
        if (main_thread && main_thread->byte_code) {
            top_thread = main_thread;

            auto result = vm.execute(main_thread);
            
            if (vm.is_suspended) {  // Исправлено пустое условие
                status = ProcessStatus::SUSPENDED;
            }
        }

        // 3. Execute post handler
        execute_post_handler(vm);  // Добавлен аргумент vm

        if (status == ProcessStatus::RUNNING) {
            status = ProcessStatus::SUSPENDED;
        }

        return true;
    }


    // ============================================================================
    // Control
    // ============================================================================

    void Process::activate(Process* active_pool, StringId newname, std::shared_ptr<StackFrame> stack_top) {
        name = newname;
        active_pool->add_child(this);
        
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
        if (!main_thread && current_state && current_state->get_code_function()) {
            main_thread = std::make_shared<StateFrame>(current_state, stack_top);
        }

        top_thread = main_thread;
        status = ProcessStatus::READY;

        // Создаем StateFrame для текущего состояния
        if (current_state && !current_state_frame) {
            current_state_frame = std::make_shared<StateFrame>(current_state, stack_frame_top);
            push_frame(current_state_frame);
            
            // Выполняем enter-обработчик
            auto& vm = Kernel::instance().virtual_machine();
            execute_enter_handler(vm, current_state->get_enter_function());
        }

        // Обновляем обработчики
        auto& vm = Kernel::instance().virtual_machine();
        update_state_hooks(vm);

        lg::debug("Process '{}': activated with state '{}'",
            get_name_string(),
            current_state ? current_state->name.to_string() : "null");
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

    // ============================================================================
    // Connection Management
    // ============================================================================

    void Process::connect_to(Process* other, StringId connection_type) {
        if (!other) return;

        // TODO: Реализовать логику соединений
        lg::debug("Process '{}': connected to '{}' with type '{}'",
            get_name_string(),
            other->get_name_string(),
            connection_type);
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
    // Private Methods
    // ============================================================================

    bool Process::execute_immediate_transition(StateDesc* new_state) {
        // Для немедленного перехода выполняем всё в текущем контексте
        if (!new_state) return false;
        
        // Сохраняем текущее состояние
        StateDesc* old_state = current_state;
        
        // Выходим из текущего состояния
        if (current_state_frame) {
            // Вызываем exit обработчик
            current_state_frame->exit();
            current_state_frame = nullptr;
        }
        
        // Устанавливаем новое состояние
        current_state = new_state;
        
        // Копируем делегаты
        trans_hook = new_state->get_trans_function();
        post_hook = new_state->get_post_function();
        event_handler = new_state->get_event_handler();
        
        // Создаем новый фрейм состояния
        current_state_frame = std::make_shared<StateFrame>(new_state, stack_frame_top);
        push_frame(current_state_frame);
        
        // Выполняем enter обработчик немедленно
        auto enter = new_state->get_enter_function();
        if (enter) {
            auto& vm = Kernel::instance().virtual_machine();
            execute_enter_handler(vm, enter);
        }
        
        lg::debug("Process '{}': immediate transition from '{}' to '{}'",
            get_name_string(),
            old_state ? old_state->name.to_string() : "null",
            new_state->name.to_string());
        
        return true;
    }

    bool Process::execute_deferred_transition(StateDesc* new_state) {
        auto& vm = Kernel::instance().virtual_machine();
        
        // Удаляем текущий StateFrame (вызовет exit-обработчик)
        if (current_state_frame) {
            // Ищем StateFrame в стеке
            auto frame = stack_frame_top;
            while (frame && frame != current_state_frame) {
                frame = frame->parent;
            }

            if (frame == current_state_frame) {
                pop_frame(); // Вызовет exit-обработчик
            }

            current_state_frame = nullptr;
        }

        // Обновляем текущее состояние
        StateDesc* old_state = current_state;
        current_state = new_state;
        
        // Копируем делегаты из StateDesc в процесс
        trans_hook = new_state->get_trans_function();
        post_hook = new_state->get_post_function();
        event_handler = new_state->get_event_handler();

        // Создаем новый StateFrame
        current_state_frame = std::make_shared<StateFrame>(new_state, stack_frame_top);
        push_frame(current_state_frame);

        // Выполняем enter-обработчик нового состояния
        auto enter = new_state->get_enter_function();
        if (enter) {
            execute_enter_handler(vm, enter);
        }

        // Обновляем главный поток если нужно
        if (main_thread && new_state->get_code_function()) {
            // Обновляем байткод главного потока
            main_thread->byte_code = new_state->get_code_function();
        }

        lg::debug("Process '{}': transitioned from '{}' to '{}'",
            get_name_string(),
            old_state ? old_state->name.to_string() : "null",
            new_state->name.to_string());

        return true;
    }


    void Process::cleanup() {
        // Очищаем состояние
        current_state = nullptr;
        next_state = nullptr;

        // Очищаем обработчики
        trans_hook = nullptr;
        post_hook = nullptr;
        event_handler = nullptr;

        // Сбрасываем статус
        status = ProcessStatus::DEAD;

        // Очищаем маски (кроме PROCESS_TREE если это узел)
        if (!has_mask(ProcessMask::PROCESS_TREE)) {
            mask = ProcessMask::NONE;
        }

        lg::debug("Process '{}': cleaned up", get_name_string());
    }

    // ============================================================================
    // State Hooks Execution (create temporary frames, use local pointers only)
    // ============================================================================
    
    void Process::execute_enter_handler(VirtualMachine& vm, FunctionDesc* enter_hook) {
        if (!enter_hook) return;
        
        // Create temporary frame for post handler
        auto enter_frame = std::make_shared<StackFrame>(
            enter_hook,
            nullptr,
            StackFrame::FrameType::GENERIC,
            StringIds::enter
        );
        
        // Save current top thread
        auto saved_top_thread = top_thread;
        top_thread = enter_frame;
        
        // Execute post handler (temporary, doesn't suspend)
        vm.execute(enter_frame);
        
        // Restore top thread
        top_thread = saved_top_thread;
        
        // Clean up temporary frame
        enter_frame.reset();
    }

    void Process::execute_trans_handler(VirtualMachine& vm) {
        if (!trans_hook) return;
        
        // Create temporary frame for trans handler
        auto trans_frame = std::make_shared<StackFrame>(
            trans_hook,
            nullptr,
            StackFrame::FrameType::GENERIC,
            StringIds::trans
        );
        
        // Save current top thread
        auto saved_top_thread = top_thread;
        top_thread = trans_frame;
        
        // Execute trans handler (temporary, doesn't suspend)
        vm.execute(trans_frame);
        
        // Restore top thread
        top_thread = saved_top_thread;
        
        // Clean up temporary frame
        trans_frame.reset();
    }

    void Process::execute_post_handler(VirtualMachine& vm) {
        if (!post_hook) return;
        
        // Create temporary frame for post handler
        auto post_frame = std::make_shared<StackFrame>(
            post_hook,
            nullptr,
            StackFrame::FrameType::GENERIC,
            StringIds::post
        );
        
        // Save current top thread
        auto saved_top_thread = top_thread;
        top_thread = post_frame;
        
        // Execute post handler (temporary, doesn't suspend)
        vm.execute(post_frame);
        
        // Restore top thread
        top_thread = saved_top_thread;
        
        // Clean up temporary frame
        post_frame.reset();
    }

    // ============================================================================
    // Utility Methods
    // ============================================================================

    std::string Process::to_string() const {
        return std::format("Process(pid:{}, name:'{}', status:{}, state:'{}')",
            pid,
            get_name_string(),
            static_cast<int>(status),
            current_state ? current_state->name.to_string() : "null"
        );
    }

    void Process::dump_info() const {
        lg::debug("=== Process Info ===");
        lg::debug("  PID: {}", pid);
        lg::debug("  Name: '{}'", get_name_string());
        lg::debug("  Status: {}", static_cast<int>(status));
        lg::debug("  Mask: 0x{:08x}", static_cast<u32>(mask));
        lg::debug("  State: '{}'", current_state ? current_state->name : "null");
        lg::debug("  Next State: '{}'", next_state ? next_state->name : "null");
        lg::debug("  Heap: {}/{} bytes", heap_used(), heap_size());
        lg::debug("  Stack Depth: {}", [this]() {
            u32 depth = 0;
            StackFrame* frame = stack_frame_top.get();
            while (frame) { depth++; frame = frame->parent.get(); }
            return depth;
            }());
        lg::debug("  Children: {}", [this]() {
            u32 count = 0;
            Process* child = this->child;
            while (child) { count++; child = child->brother; }
            return count;
            }());
    }

} // namespace carbon::kernel