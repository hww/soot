# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

add_library(z80_lisp_lib
    src/lib/object.cpp
    src/lib/interpreter.cpp  
    src/lib/reader.cpp
    src/lib/source-info.cpp
    src/lib/crc32.cpp
)

# Создаем тестовый исполняемый файл
add_executable(z80_lisp_test
    tests/test_main.cpp
    tests/core/test_basic_types.cpp
    tests/core/test_arithmetic.cpp
    tests/core/test_lists.cpp
    tests/core/test_special_forms.cpp
    tests/core/test_predicates.cpp
    tests/core/test_logical.cpp
    tests/core/test_strings.cpp
    tests/core/test_vectors.cpp
    tests/core/test_hash_tables.cpp
    tests/core/test_macros.cpp
    tests/core/test_quasiquote.cpp
    tests/core/test_loops.cpp
    tests/core/test_conversions.cpp
    tests/core/test_math.cpp
    tests/core/test_system.cpp
)

# Линкуем с нашей библиотекой и gtest
target_link_libraries(z80_lisp_test z80_lisp_lib gtest_main fmt)
target_include_directories(z80_lisp_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/lib)

if(MSVC)
    set_target_properties(z80_lisp_test PROPERTIES 
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/Debug"
    )
endif()

# Регистрируем тесты для Test Explorer
gtest_discover_tests(z80_lisp_test)