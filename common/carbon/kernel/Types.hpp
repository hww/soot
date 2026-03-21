#pragma once

#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"

namespace runtime::kernel {

    // ============================================================================
    // Process Status Enum
    // ============================================================================

    /// Статусы выполнения процесса (аналогично OpenGoal)
    enum class ProcessStatus {
        DEAD,           // Процесс в пуле, можно переиспользовать
        READY,          // Активирован и готов к выполнению  
        RUNNING,        // Выполняется прямо сейчас
        SUSPENDED,      // Приостановлен (yield), ждет следующего кадра
        WAITING_TO_RUN, // Ожидает первого запуска
        INITIALIZE,     // В процессе инициализации
        INITIALIZE_GO   // Инициализация с отложенным переходом
    };

    // ============================================================================
    // Process Mask Enum  
    // ============================================================================

    /// Маски процессов для управления выполнением (аналогично OpenGoal и C#)
    enum class ProcessMask : u32 {
        NONE = 0,
        EXECUTE = 1 << 0,      // Блокирует выполнение процесса
        SLEEP = 1 << 1,        // Процесс спит (можно разбудить)
        SLEEP_CODE = 1 << 2,   // Не выполнять основной код, только trans/post
        PROCESS_TREE = 1 << 3, // Это узел дерева, а не реальный процесс
        PAUSE = 1 << 4,        // Не выполнять при паузе игры
        MENU = 1 << 5,         // Не выполнять когда открыто меню  
        GOING = 1 << 6,        // Установлен следующий состояние (pending transition)
        HEAP_SHRUNK = 1 << 7   // Куча процесса уже была сжата
    };

    ENUM_FLAG_OPERATORS(ProcessMask);


    // Битровые операции для масок
    //inline ProcessMask operator|(ProcessMask a, ProcessMask b) {
    //    return static_cast<ProcessMask>(static_cast<u32>(a) | static_cast<u32>(b));
    //}
    //
    //inline ProcessMask operator&(ProcessMask a, ProcessMask b) {
    //    return static_cast<ProcessMask>(static_cast<u32>(a) & static_cast<u32>(b));
    //}
    //
    //inline ProcessMask operator~(ProcessMask a) {
    //    return static_cast<ProcessMask>(~static_cast<u32>(a));
    //}

    // ============================================================================
    // Thread Type Enum
    // ============================================================================

    /// Типы потоков выполнения (для определения контекста)
    enum class ThreadType {
        MAIN,       // Основной поток выполнения (code)
        TRANS,      // Trans-обработчик (выполняется перед code)
        POST,       // Post-обработчик (выполняется после code)
        EVENT,      // Event-обработчик (обработка сообщений)
        NATIVE      // Нативная функция C++
    };

} // namespace runtime::kernel