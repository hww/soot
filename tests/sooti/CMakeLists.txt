# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/script)

# ==============================================================
# Script
# ==============================================================

# Создаем тестовый исполняемый файл
add_executable(sooti_test
    tests/sooti/MainScriptTest.cpp
    tests/sooti/TestBasicTypes.cpp
    tests/sooti/TestArithmetic.cpp
    tests/sooti/TestLists.cpp
    tests/sooti/TestSpecialForms.cpp
    tests/sooti/TestPredicates.cpp
    tests/sooti/TestLogical.cpp
    tests/sooti/TestStrings.cpp
    tests/sooti/TestVectors.cpp
    tests/sooti/TestHashTables.cpp
    tests/sooti/TestMacros.cpp
    tests/sooti/TestQuasiquote.cpp
    tests/sooti/TestLoops.cpp
    tests/sooti/TestConversions.cpp
    tests/sooti/TestMath.cpp
    tests/sooti/TestSystem.cpp
)

# Линкуем с нашей библиотекой и gtest
target_link_libraries(sooti_test libsoti gtest_main fmt)
target_include_directories(sooti_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/script)

if(MSVC)
    set_target_properties(sooti_test PROPERTIES 
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/Debug"
    )
endif()
set_target_properties(sooti_test PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
# Регистрируем тесты для Test Explorer
gtest_discover_tests(sooti_test)


