add_library(libcarbonc
    common/carbonc/compiler/Env.cpp
)

# Исправлено: используем libsot вместо repl
target_include_directories(libcarbonc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(libcarbonc PRIVATE libsoti fmt typ)

if(WIN32)
    target_link_libraries(libsot PRIVATE ws2_32)
else()
    target_link_libraries(libsot PRIVATE pthread)
endif()

add_executable(carbonc common/carbonc/Main.cpp)

# Исправлено: добавляем связывание с libsot
target_link_libraries(carbonc PRIVATE libsoti libsootutils libcarbonc libtypesys)

set_target_properties(carbonc PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

install(TARGETS carbonc
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)


