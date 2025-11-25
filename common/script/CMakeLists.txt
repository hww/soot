
# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(script_lib 
    common/util/crc32.cpp
    common/util/assert.cpp
    common/util/file_util.cpp
    common/util/print_float.cpp
    common/util/log.cpp

    common/script/text_db.cpp
    common/script/object.cpp
    common/script/reader.cpp
    common/script/interpreter.cpp
    common/script/printer.cpp
    common/script/printer_env.cpp
    common/script/pretty_printer.cpp
    common/script/parse_helpers.cpp
)

# Исполняемый файл
add_executable(lisp common/script/main.cpp)
target_link_libraries(lisp PRIVATE script_lib  fmt)
set_target_properties(lisp PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_target_properties(lisp PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
