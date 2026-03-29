# common/sootc/CMakeLists.txt

# Библиотека компилятора
add_library(libsootc
    common/sootc/Main.cpp
    common/sootc/Compiler/FunctionCompiler.cpp
    common/sootc/Compiler/TypeCompiler.cpp
    common/sootc/Compiler/Compiler.cpp
    common/sootc/Compiler/Env.cpp
    common/sootc/IR/IR_Move.cpp
    common/sootc/IR/IR_Value.cpp
    common/sootc/IR/IR_Math.cpp    
)

# Пути для include
target_include_directories(libsootc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/common/util
)

# Связывание с зависимостями
target_link_libraries(libsootc PRIVATE 
    libsoti 
    libsoticore 
    libtypesys
    libsootutils 
    fmt 
    replxx 
    xsockets_lib
    libcarbon
    pthread
)

if(WIN32)
    target_link_libraries(libsootc PRIVATE ws2_32)
endif()

# Исполняемый файл для тестирования компилятора
add_executable(sootc common/sootc/Main.cpp)

# ВАЖНО: добавляем все библиотеки, которые нужны исполняемому файлу
target_link_libraries(sootc PRIVATE 
    libsootc
    libsoti 
    libsoticore 
    libtypesys
    libsootutils 
    fmt 
    replxx 
    xsockets_lib
    libcarbon
    pthread
)

if(WIN32)
    target_link_libraries(sootc PRIVATE ws2_32)
endif()

set_target_properties(sootc PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Установка
install(TARGETS sootc
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)