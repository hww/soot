# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/carbon)

# ==============================================================
# Script
# ==============================================================

# Создаем тестовый исполняемый файл
add_executable(carbon_test
    tests/carbon/TestTypes.cpp
    tests/carbon/TestStringId.cpp
    tests/carbon/TestVariant.cpp
    tests/carbon/TestInstructions.cpp
    tests/carbon/TestStackFrame.cpp
    tests/carbon/TestBinaryFile.cpp
    tests/carbon/TestScriptObject.cpp
    tests/carbon/TestNativeFunctions.cpp
    tests/carbon/TestVirtualMachine.cpp
    tests/carbon/TestPtr.cpp
)

# Линкуем с нашей библиотекой и gtest
target_link_libraries(carbon_test libcarbon gtest_main fmt)
target_include_directories(carbon_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/script)

if(MSVC)
    set_target_properties(carbon_test PROPERTIES 
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/Debug"
    )
endif()

# Регистрируем тесты для Test Explorer
gtest_discover_tests(carbon_test)


