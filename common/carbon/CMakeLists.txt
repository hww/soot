
# Создаем нашу библиотеку БЕЗ replxx файлов

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common/carbon/defconstruct/include)

add_library(libcarbon 
    # External
    common/util/Crc32.cpp
    common/util/Assert.cpp
    common/util/FileUtil.cpp
    common/util/PrintFloat.cpp
    common/util/Log.cpp

    ## Local Libs
    common/carbon/lib/StringIdManager.cpp
    common/carbon/lib/Ptr.cpp

    ## Defconstruct
    common/carbon/defconstruct/source/base/sidbase.cpp
    common/carbon/defconstruct/source/base/binaryfile.cpp
    common/carbon/defconstruct/source/disassembly/instructions.cpp
    common/carbon/defconstruct/source/ast/type.cpp
    common/carbon/defconstruct/source/compilation/program_binary_element.cpp
    common/carbon/defconstruct/source/compilation/function.cpp

    ## Files
    common/carbon/files/RelocatableBuffer.cpp
    common/carbon/files/Definition.cpp
    common/carbon/files/StateDesc.cpp
    common/carbon/files/TypeDesc.cpp
    common/carbon/files/FunctionDesc.cpp
    common/carbon/files/BinaryFile.cpp   
    common/carbon/files/BinaryFileBuilder.cpp

    ## Modules
    common/carbon/modules/Module.cpp
    common/carbon/modules/ModuleRegistry.cpp
    common/carbon/modules/ModuleManager.cpp

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

target_compile_features(libcarbon PRIVATE cxx_std_23)

get_target_property(carbon_std libcarbon CXX_STANDARD)
message(STATUS "libcarbon C++ standard: ${carbon_std}")

# Исполняемый файл для тестирования VM
add_executable(carbon common/carbon/Main.cpp)
target_compile_features(carbon PRIVATE cxx_std_23)

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
