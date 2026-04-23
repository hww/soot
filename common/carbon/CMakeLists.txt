
# Создаем нашу библиотеку БЕЗ replxx файлов

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common/carbon)


add_library(libcarbon 
    # External
    common/util/StringIdHash.cpp
    common/util/Assert.cpp
    common/util/FileUtil.cpp
    common/util/PrintFloat.cpp
    common/util/Log.cpp

    ## Local Libs
    common/carbon/lib/SIDBase.cpp
    common/carbon/lib/StringIdManager.cpp
    common/carbon/lib/Ptr.cpp

    ## File
    common/carbon/file/BinaryFile.cpp   
    common/carbon/file/BinaryFileInspector.cpp   
    common/carbon/file/ProgramBinaryElement.cpp   

    ## Virtual machine
    common/carbon/vm/Instructions.cpp
    common/carbon/vm/StackFrame.cpp
    common/carbon/vm/VirtualMachine.cpp

    ## Kernel
    common/carbon/kernel/EventMessage.cpp   
    common/carbon/kernel/NativeFunc.cpp   
    common/carbon/kernel/Connection.cpp
    common/carbon/kernel/Connectable.cpp
    common/carbon/kernel/Engine.cpp
    common/carbon/kernel/Kernel.cpp
    common/carbon/kernel/Process.cpp
    common/carbon/kernel/ProcessRunner.cpp
    common/carbon/kernel/Scheduler.cpp
    common/carbon/kernel/StateFrame.cpp
)



# Исполняемый файл для тестирования VM
add_executable(carbon common/carbon/Main.cpp)


target_link_libraries(carbon PRIVATE 
    libsot 
    libsotcore 
    libtypesys
    libsootutils 
    fmt 
    libcarbon
)
if(WIN32)
    target_link_libraries(carbon PRIVATE ws2_32)
else()
    target_link_libraries(carbon PRIVATE pthread)
endif()

set_target_properties(carbon PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
