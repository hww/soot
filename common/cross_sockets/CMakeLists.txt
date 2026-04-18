# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(xsockets_lib 
    common/cross_sockets/XSocket.cpp
    common/cross_sockets/XSocketClient.cpp
    common/cross_sockets/XSocketServer.cpp)


target_include_directories(xsockets_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(xsockets_lib PRIVATE ws2_32)
endif() 