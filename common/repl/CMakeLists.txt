include(common/cross_sockets/CMakeLists.txt)

add_library(repl_lib
    common/repl/Config.cpp
    common/repl/ReplWrapper.cpp
    common/repl/nrepl/ReplServer.cpp
    common/repl/nrepl/ReplClient.cpp
)

# Исправлено: используем repl_lib вместо repl
target_include_directories(repl_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(repl_lib PRIVATE script_lib fmt replxx xsockets_lib)

if(WIN32)
    target_link_libraries(repl_lib PRIVATE ws2_32)
else()
    target_link_libraries(repl_lib PRIVATE pthread)
endif()

add_executable(soot common/repl/Main.cpp)
# Исправлено: добавляем связывание с repl_lib
target_link_libraries(soot PRIVATE repl_lib)
set_target_properties(soot PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)


install(TARGETS soot
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)


# Определяем путь для ресурсов (обычно /usr/local/share/soot)
set(SOOT_SHARE_DIR "share/soot")

# Установка системной библиотеки (базовые функции)
install(FILES 
    lib.sot 
    DESTINATION ${SOOT_SHARE_DIR}
)

# Установка шаблонов конфигурации (чтобы пользователь мог их скопировать в ~/.config/soot)
# Мы кладем их в подпапку skeletons или examples
install(FILES 
    config.sot 
    startup-pre.sot 
    startup-post.sot
    DESTINATION ${SOOT_SHARE_DIR}/skeletons
)
