add_library(libxiff
   common/xiff/src/XiffCompiler.cpp
   common/xiff/src/XiffInjector.cpp
   common/xiff/src/XiffCompiler.cpp
)

# Исправлено: используем repl_lib вместо repl
target_include_directories(libxiff PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common/xiff/include
)

target_link_libraries(libxiff PRIVATE script_lib fmt replxx )

if(WIN32)
    target_link_libraries(libxiff PRIVATE ws2_32)
else()
    target_link_libraries(libxiff PRIVATE pthread)
endif()

add_executable(xiff common/xiff/src/XiffMain.cpp)
# Исправлено: добавляем связывание с repl_lib
target_link_libraries(xiff PRIVATE libxiff)
set_target_properties(xiff PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Installation (optional)
install(TARGETS xiff
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Путь к тестовому файлу
set(TEST_ASM_FILE "${CMAKE_SOURCE_DIR}/common/xiff/examples/test.sot.asm")
set(OUTPUT_HEADER "${CMAKE_BINARY_DIR}/common/xiff/out/interface.h")
set(libxiff_FILE "${CMAKE_SOURCE_DIR}/common/xiff/soot/xiff-core.sot")

add_custom_target(run
    COMMAND xiff -v -l ${libxiff_FILE} -o ${OUTPUT_HEADER} ${TEST_ASM_FILE}
    DEPENDS xiff
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "XIFF: Generating ${OUTPUT_HEADER} from ${TEST_ASM_FILE}"
    USES_TERMINAL
)

# Чтобы 'run' всегда зависел от того, собран ли сам xiff
add_dependencies(run xiff)