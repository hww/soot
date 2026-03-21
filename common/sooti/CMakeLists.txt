# common/sooti/CMakeLists.txt

# 1. Ядро: только базовые типы данных Лиспа
add_library(libsoticore
    common/sooti/Object.cpp
    common/sooti/SourceText.cpp
    common/sooti/Reader.cpp
    common/sooti/Printer.cpp
    common/sooti/TextDb.cpp
    common/sooti/ParseHelpers.cpp
    common/sooti/PrinterEnv.cpp
    common/sooti/PrettyPrinter.cpp
    common/sooti/Errors.cpp
    common/sooti/Archive.cpp
)

target_include_directories(libsoticore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common/util
)

target_link_libraries(libsoticore PRIVATE 
    fmt::fmt 
    libsootutils
)

# 2. Архивные модули
add_library(libsoti
    common/sooti/Interpreter.cpp
    common/sooti/archive/MemoryArchive.cpp
    common/sooti/archive/RelocationTable.cpp
    common/sooti/archive/MemorySymbolTable.cpp
    common/sooti/archive/MemoryRegion.cpp
    common/sooti/archive/MemoryBuffer.cpp
    common/sooti/archive/LabelTable.cpp
)

target_include_directories(libsoti PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common/util
)

target_link_libraries(libsoti PRIVATE 
    libsoticore
    fmt::fmt 
    libsootutils
)

# 3. Исполняемый файл
add_executable(sooti common/sooti/SOOTiMain.cpp)
target_link_libraries(sooti PRIVATE libsoti libtypesys fmt::fmt libsootutils)

set_target_properties(sooti PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Установка
install(TARGETS sooti
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)