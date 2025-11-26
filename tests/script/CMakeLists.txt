# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/script)

# ==============================================================
# Script
# ==============================================================

# Создаем тестовый исполняемый файл
add_executable(script_test
    tests/script/MainScriptTest.cpp
    tests/script/TestBasicTypes.cpp
    tests/script/TestArithmetic.cpp
    tests/script/TestLists.cpp
    tests/script/TestSpecialForms.cpp
    tests/script/TestPredicates.cpp
    tests/script/TestLogical.cpp
    tests/script/TestStrings.cpp
    tests/script/TestVectors.cpp
    tests/script/TestHashTables.cpp
    tests/script/TestMacros.cpp
    tests/script/TestQuasiquote.cpp
    tests/script/TestLoops.cpp
    tests/script/TestConversions.cpp
    tests/script/TestMath.cpp
    tests/script/TestSystem.cpp
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


