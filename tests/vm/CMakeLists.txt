# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/vm)

# ==============================================================
# Script
# ==============================================================

# Создаем тестовый исполняемый файл
add_executable(vm_test
    tests/vm/test_types.cpp
    tests/vm/test_string_id.cpp
    tests/vm/test_variant.cpp
    tests/vm/test_instructions.cpp
    tests/vm/test_stack_frame.cpp
    tests/vm/test_binary_file.cpp
    tests/vm/test_script_object.cpp
    tests/vm/test_native_functions.cpp
    tests/vm/test_virtual_machine.cpp
    tests/vm/test_module_pool.cpp
    tests/vm/test_ptr.cpp
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


