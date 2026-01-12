
# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(script_lib 
    common/util/Crc32.cpp
    common/util/Assert.cpp
    common/util/FileUtil.cpp
    common/util/PrintFloat.cpp
    common/util/Log.cpp

    common/script/TextDb.cpp
    common/script/Object.cpp
    common/script/Reader.cpp
    common/script/Interpreter.cpp
    common/script/Printer.cpp
    common/script/PrinterEnv.cpp
    common/script/PrettyPrinter.cpp
    common/script/ParseHelpers.cpp
)

# Исполняемый файл
add_executable(sooti common/script/Main.cpp)
target_link_libraries(sooti PRIVATE script_lib  fmt)
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
