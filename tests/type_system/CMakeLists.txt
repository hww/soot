# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/script)

# ==============================================================
# Type System
# ==============================================================
add_executable(type_system_tests
    tests/type_system/test_defenum.cpp
    tests/type_system/test_type_system.cpp
    tests/type_system/test_deftype.cpp
    #tests/type_system/test_goos_ts.cpp
)
# Тестовый исполняемый файл
target_link_libraries(type_system_tests type_system_lib script_lib gtest_main fmt)
target_include_directories(type_system_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/script)
if(MSVC)
    set_target_properties(type_system_tests PROPERTIES 
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/Debug"
    )
endif()

# Добавляем тесты
gtest_discover_tests(type_system_tests)
 

