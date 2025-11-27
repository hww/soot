# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/vm)

# ==============================================================
# Script
# ==============================================================

# Создаем тестовый исполняемый файл
add_executable(vm_test
    tests/runtime/TestTypes.cpp
    tests/runtime/TestStringId.cpp
    tests/runtime/TestVariant.cpp
    tests/runtime/TestInstructions.cpp
    tests/runtime/TestStackFrame.cpp
    tests/runtime/TestBinaryFile.cpp
    tests/runtime/TestScriptObject.cpp
    tests/runtime/TestNativeFunctions.cpp
    tests/runtime/TestVirtualMachine.cpp
    tests/runtime/TestModulePool.cpp
    tests/runtime/TestPtr.cpp
)

# Линкуем с нашей библиотекой и gtest
target_link_libraries(vm_test vm_lib gtest_main fmt)
target_include_directories(vm_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/script)

if(MSVC)
    set_target_properties(vm_test PROPERTIES 
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/Debug"
    )
endif()

# Регистрируем тесты для Test Explorer
gtest_discover_tests(vm_test)


