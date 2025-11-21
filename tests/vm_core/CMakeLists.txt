# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/vm_core)

# ==============================================================
# Script
# ==============================================================

# Создаем тестовый исполняемый файл
add_executable(vm_core_types_test
    #tests/vm_core/types_test.cpp
    #tests/vm_core/string_id_test.cpp
    #tests/vm_core/variant_test.cpp
    #tests/vm_core/instructions_test.cpp
    #tests/vm_core/stack_frame_test.cpp
    #tests/vm_core/bytecode_test.cpp
    #tests/vm_core/virtual_machine_test.cpp
    tests/vm_core/test_script_object.cpp

    tests/vm_core/test_native_functions.cpp
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


