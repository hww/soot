
# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(vm_lib 
    # External
    common/util/Crc32.cpp
    common/util/Assert.cpp
    common/util/FileUtil.cpp
    common/util/PrintFloat.cpp
    common/util/Log.cpp
    ## Local Libs
    common/runtime/lib/StringId.cpp
    common/runtime/lib/Ptr.cpp
    ## Files
    common/runtime/files/BinaryFile.cpp
    
    common/runtime/files/BinaryFileBuilder.cpp
    ## Modules
    common/runtime/modules/Module.cpp
    common/runtime/modules/ModuleRegistry.cpp
    common/runtime/modules/ModuleManager.cpp
    ## Virtual machine
    common/runtime/vm/StackFrame.cpp
    common/runtime/vm/VirtualMachine.cpp
    ## Kernel
    common/runtime/kernel/NativeFunc.cpp
    
    common/runtime/kernel/Connection.cpp
    common/runtime/kernel/Connectable.cpp
    common/runtime/kernel/Engine.cpp
    common/runtime/kernel/Kernel.cpp
    common/runtime/kernel/Process.cpp
    common/runtime/kernel/ProcessRunner.cpp
    common/runtime/kernel/StateDefinition.cpp
    common/runtime/kernel/Scheduler.cpp
)

