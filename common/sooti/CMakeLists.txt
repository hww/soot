
# Создаем нашу библиотеку БЕЗ replxx файлов
# 1. Ядро: только базовые типы данных Лиспа
add_library(libsoticore
    common/sooti/Object.cpp
    common/sooti/Reader.cpp
    common/sooti/Printer.cpp
    common/sooti/TextDb.cpp
    common/sooti/ParseHelpers.cpp
    common/sooti/PrinterEnv.cpp
    common/sooti/PrettyPrinter.cpp
)
# 3. Высокоуровневый интерпретатор и мост к типам
add_library(libsoti
    common/sooti/Interpreter.cpp
    common/sooti/SootTypeSystem.cpp
)

target_link_libraries(libsoticore PRIVATE fmt::fmt libsootutils)

# Линкуем исполняемый файл
add_executable(sooti common/sooti/SOOTiMain.cpp)
target_link_libraries(sooti PRIVATE libsoti libtypesys libsoticore fmt::fmt  libsootutils)

set_target_properties(sooti PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_target_properties(sooti PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)


# Installation (optional)
install(TARGETS sooti
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
