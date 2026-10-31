
add_library(librepl
    ${CMAKE_CURRENT_SOURCE_DIR}/common/repl/ReplWrapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/repl/nrepl/ReplServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/repl/nrepl/ReplClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/repl/CommandTable.cpp
)


if(WIN32)
    target_link_libraries(librepl PUBLIC
        fmt 
        replxx 
        xsockets_lib
        ws2_32
    )
else()
    target_link_libraries(librepl PUBLIC
        fmt 
        replxx 
        xsockets_lib
    )
endif()