# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/vm_core)

# ==============================================================
# Script
# ==============================================================

# Создаем тестовый исполняемый файл
add_executable(vm_core_types_test
    tests/vm_core/test_types.cpp
    tests/vm_core/test_string_id.cpp
    tests/vm_core/test_variant.cpp
    tests/vm_core/test_instructions.cpp
    tests/vm_core/test_stack_frame.cpp
    tests/vm_core/test_bytecode.cpp
    tests/vm_core/test_script_object.cpp
    tests/vm_core/test_native_functions.cpp
    tests/vm_core/test_virtual_machine.cpp
)

# Линкуем с нашей библиотекой и gtest
target_link_libraries(vm_core_types_test vm_core_lib gtest_main fmt)
target_include_directories(vm_core_types_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/script)

if(MSVC)
    set_target_properties(vm_core_types_test PROPERTIES 
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/Debug"
    )
endif()

# Регистрируем тесты для Test Explorer
gtest_discover_tests(vm_core_types_test)


