# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/carbon)

# ==============================================================
# Type System
# ==============================================================
add_executable(gc_tests

    tests/gc/gc_test.cpp
)
# Тестовый исполняемый файл
target_link_libraries(gc_tests libcarbon libscript libtypesys libscriptcore gtest_main fmt)
target_include_directories(gc_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/script)
if(MSVC)
    set_target_properties(gc_tests PROPERTIES 
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/Debug"
    )
endif()

# Добавляем тесты
gtest_discover_tests(gc_tests)
 

