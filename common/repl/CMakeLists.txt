include(common/cross_sockets/CMakeLists.txt)

add_library(repl_lib
    common/repl/config.cpp
    common/repl/repl_wrapper.cpp
    common/repl/nrepl/repl_server.cpp
    common/repl/nrepl/repl_client.cpp
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

add_executable(repl common/repl/main.cpp)
# Исправлено: добавляем связывание с repl_lib
target_link_libraries(repl PRIVATE repl_lib)
set_target_properties(repl PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
