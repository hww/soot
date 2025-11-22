# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/script)

# ==============================================================
# Script
# ==============================================================

# Создаем тестовый исполняемый файл
add_executable(script_test
    tests/script/main_script_test.cpp
    tests/script/test_basic_types.cpp
    tests/script/test_arithmetic.cpp
    tests/script/test_lists.cpp
    tests/script/test_special_forms.cpp
    tests/script/test_predicates.cpp
    tests/script/test_logical.cpp
    tests/script/test_strings.cpp
    tests/script/test_vectors.cpp
    tests/script/test_hash_tables.cpp
    tests/script/test_macros.cpp
    tests/script/test_quasiquote.cpp
    tests/script/test_loops.cpp
    tests/script/test_conversions.cpp
    tests/script/test_math.cpp
    tests/script/test_system.cpp
)

# Линкуем с нашей библиотекой и gtest
target_link_libraries(script_test script_lib gtest_main fmt)
target_include_directories(script_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/script)

if(MSVC)
    set_target_properties(script_test PROPERTIES 
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/Debug"
    )
endif()
set_target_properties(script_test PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
# Регистрируем тесты для Test Explorer
gtest_discover_tests(script_test)


