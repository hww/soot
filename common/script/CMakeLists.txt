
# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(script_lib 
    common/script/object.cpp
    common/script/reader.cpp
    common/script/interpreter.cpp
    common/script/source_info.cpp
    common/script/crc32.cpp
    common/script/pretty_printer.cpp
    common/script/parse_helpers.cpp
    common/util/assert.cpp
    common/log/log.cpp)

# Исполняемый файл
add_executable(lisp common/script/main.cpp)
target_link_libraries(lisp PRIVATE script_lib  fmt)
set_target_properties(lisp PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
