# common/soot/CMakeLists.txt

# 1. Ядро: только базовые типы данных Лиспа
add_library(libsotcore
    common/soot/Object.cpp
    common/soot/SourceText.cpp
    common/soot/Reader.cpp
    common/soot/Printer.cpp
    common/soot/TextDb.cpp
    common/soot/ParseHelpers.cpp
    common/soot/PrinterEnv.cpp
    common/soot/PrettyPrinter.cpp
    common/soot/Errors.cpp
    common/soot/Archive.cpp
)

target_include_directories(libsotcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common/util
)

target_link_libraries(libsotcore PRIVATE 
    fmt::fmt 
    libsootutils
    replxx  
    librepl          
)

# 2. Архивные модули
add_library(libsot
    common/soot/Interpreter.cpp
    common/soot/archive/MemoryArchive.cpp
    common/soot/archive/RelocationTable.cpp
    common/soot/archive/MemorySymbolTable.cpp
    common/soot/archive/MemoryRegion.cpp
    common/soot/archive/MemoryBuffer.cpp
    common/soot/archive/LabelTable.cpp
)

target_include_directories(libsot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common/util
)

target_link_libraries(libsot PRIVATE 
    libsotcore
    fmt::fmt 
    replxx  
    librepl          
    libsootutils
)

# 3. Исполняемый файл
add_executable(soot common/soot/Main.cpp)
target_link_libraries(soot PRIVATE 
    libsot 
    libtypesys 
    fmt::fmt 
    replxx      
    librepl     
    libsootutils
)

set_target_properties(soot PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Установка
install(TARGETS soot
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)