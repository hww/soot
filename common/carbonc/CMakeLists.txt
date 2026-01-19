add_library(libcarbonc
    common/soot/Config.cpp
    common/soot/ReplWrapper.cpp
    common/soot/nrepl/ReplServer.cpp
    common/soot/nrepl/ReplClient.cpp
)

# Исправлено: используем libsot вместо repl
target_include_directories(libcarbonc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(libsot PRIVATE libcarbonc libsoti fmt )

if(WIN32)
    target_link_libraries(libsot PRIVATE ws2_32)
else()
    target_link_libraries(libsot PRIVATE pthread)
endif()

add_executable(soot common/soot/Main.cpp)

# Исправлено: добавляем связывание с libsot
target_link_libraries(carbonc PRIVATE libsoti libcarbonc)
set_target_properties(carbonc PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)


install(TARGETS carbonc
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)


