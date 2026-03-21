
# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(libcarbon 
    # External
    common/util/Crc32.cpp
    common/util/Assert.cpp
    common/util/FileUtil.cpp
    common/util/PrintFloat.cpp
    common/util/Log.cpp
    ## Local Libs
    common/carbon/lib/StringId.cpp
    common/carbon/lib/Ptr.cpp
    ## Files
    common/carbon/files/BinaryFile.cpp
    
    common/carbon/files/BinaryFileBuilder.cpp
    ## Modules
    common/carbon/modules/Module.cpp
    common/carbon/modules/ModuleRegistry.cpp
    common/carbon/modules/ModuleManager.cpp
    ## Virtual machine
    common/carbon/vm/StackFrame.cpp
    common/carbon/vm/VirtualMachine.cpp
    ## Kernel
    common/carbon/kernel/NativeFunc.cpp
    
    common/carbon/kernel/Connection.cpp
    common/carbon/kernel/Connectable.cpp
    common/carbon/kernel/Engine.cpp
    common/carbon/kernel/Kernel.cpp
    common/carbon/kernel/Process.cpp
    common/carbon/kernel/ProcessRunner.cpp
    common/carbon/kernel/StateDefinition.cpp
    common/carbon/kernel/Scheduler.cpp
)
# Исполняемый файл для тестирования VM
add_executable(carbon common/carbon/Main.cpp)

target_link_libraries(carbon PRIVATE 
    libsoti 
    libsoticore 
    libtypesys
    libsootutils 
    fmt 
    libcarbon
    pthread
)

if(WIN32)
    target_link_libraries(carbon PRIVATE ws2_32)
endif()

set_target_properties(carbon PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
