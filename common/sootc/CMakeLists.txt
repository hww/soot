# common/sootc/CMakeLists.txt

# Библиотека компилятора
add_library(libsootc
    common/sootc/node/Node.cpp
    common/sootc/node/FunctionNode.cpp
    common/sootc/compiler/NodeBuilder.cpp
    common/sootc/compiler/Compiler.cpp
    common/sootc/compiler/FunctionCompiler.cpp
    common/sootc/compiler/FileCompiler.cpp
    #common/sootc/Main.cpp
    #common/sootc/Compiler/Compiler.cpp
    #common/sootc/Env/Env.cpp
    #common/sootc/Env/GlobalEnv.cpp
    #common/sootc/Env/FileEnv.cpp
    #common/sootc/Env/FunctionEnv.cpp
    #common/sootc/Env/TypeEnv.cpp

    #common/sootc/IR/IR_Value.cpp
    #common/sootc/IR/IR_FunctionValue.cpp
    #common/sootc/IR/IR_Expression.cpp    

    #common/sootc/IR/StaticObject.cpp
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
)
if(WIN32)
    target_link_libraries(libsootc PRIVATE ws2_32)
else()
    target_link_libraries(libsootc PRIVATE pthread)
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
)

if(WIN32)
    target_link_libraries(sootc PRIVATE ws2_32)
else()
    target_link_libraries(sootc PRIVATE ptread)
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