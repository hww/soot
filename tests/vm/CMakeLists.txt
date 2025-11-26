# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/vm)

# ==============================================================
# Script
# ==============================================================

# Создаем тестовый исполняемый файл
add_executable(vm_test
    tests/vm/TestTypes.cpp
    tests/vm/TestStringId.cpp
    tests/vm/TestVariant.cpp
    tests/vm/TestInstructions.cpp
    tests/vm/TestStackFrame.cpp
    tests/vm/TestBinaryFile.cpp
    tests/vm/TestScriptObject.cpp
    tests/vm/TestNativeFunctions.cpp
    tests/vm/TestVirtualMachine.cpp
    tests/vm/TestModulePool.cpp
    tests/vm/TestPtr.cpp
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


