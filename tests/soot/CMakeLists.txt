# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/script)

# ==============================================================
# Script
# ==============================================================

# Создаем тестовый исполняемый файл
add_executable(soot_test
    tests/soot/MainScriptTest.cpp
    tests/soot/TestBasicTypes.cpp
    tests/soot/TestArithmetic.cpp
    tests/soot/TestLists.cpp
    tests/soot/TestSpecialForms.cpp
    tests/soot/TestPredicates.cpp
    tests/soot/TestLogical.cpp
    tests/soot/TestStrings.cpp
    tests/soot/TestVectors.cpp
    tests/soot/TestHashTables.cpp
    tests/soot/TestMacros.cpp
    tests/soot/TestQuasiquote.cpp
    tests/soot/TestLoops.cpp
    tests/soot/TestConversions.cpp
    tests/soot/TestMath.cpp
    tests/soot/TestSystem.cpp
)

# Линкуем с нашей библиотекой и gtest
target_link_libraries(soot_test libsot libtypesys gtest_main fmt)
target_include_directories(soot_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/script)

if(MSVC)
    set_target_properties(soot_test PROPERTIES 
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/Debug"
    )
endif()
set_target_properties(soot_test PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
# Регистрируем тесты для Test Explorer
gtest_discover_tests(soot_test)


