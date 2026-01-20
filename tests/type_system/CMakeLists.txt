# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/sooti)

# ==============================================================
# Type System
# ==============================================================
add_executable(typesys_tests
    tests/type_system/MainTypeSystemTest.cpp
    tests/type_system/TestDefenum.cpp
    tests/type_system/TestTypeSystem.cpp
    tests/type_system/TestDeftype.cpp
    #tests/type_system/test_goos_ts.cpp
)
# Тестовый исполняемый файл
target_link_libraries(typesys_tests libsoti libtypesys libsoticore gtest_main fmt)
target_include_directories(typesys_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/script)
if(MSVC)
    set_target_properties(typesys_tests PROPERTIES 
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/Debug"
    )
endif()

# Добавляем тесты
gtest_discover_tests(typesys_tests)
 

