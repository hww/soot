# common/script/CMakeLists.txt

# 1. Ядро: только базовые типы данных Лиспа
add_library(libsotcore
    common/script/Object.cpp
    common/script/SourceText.cpp
    common/script/Reader.cpp
    common/script/Printer.cpp
    common/script/TextDb.cpp
    common/script/ParseHelpers.cpp
    common/script/PrinterEnv.cpp
    common/script/PrettyPrinter.cpp
    common/script/Errors.cpp
    common/script/Archive.cpp
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
    common/script/Interpreter.cpp
    common/script/archive/MemoryArchive.cpp
    common/script/archive/RelocationTable.cpp
    common/script/archive/MemorySymbolTable.cpp
    common/script/archive/MemoryRegion.cpp
    common/script/archive/MemoryBuffer.cpp
    common/script/archive/LabelTable.cpp
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
add_executable(soot common/script/Main.cpp)
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