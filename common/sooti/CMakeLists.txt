
# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(libsoti 
    common/sooti/TextDb.cpp
    common/sooti/Object.cpp
    common/sooti/Reader.cpp
    common/sooti/Interpreter.cpp
    common/sooti/Printer.cpp
    common/sooti/PrinterEnv.cpp
    common/sooti/PrettyPrinter.cpp
    common/sooti/ParseHelpers.cpp
)

# Исполняемый файл
add_executable(sooti common/sooti/SOOTiMain.cpp)
target_link_libraries(sooti PRIVATE libsoti fmt libsotutil)
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
