# tests/CMakeLists.txt

# Добавляем эту строку в самое начало
include(GoogleTest)

include_directories(common/engine)

# ==============================================================
# Script
# ==============================================================

# Создаем тестовый исполняемый файл
add_executable(engine_test
    tests/engine/EngineResultTest.cpp
    tests/engine/ConnectableTest.cpp
    #tests/engine/ConnectionTest.cpp
    #tests/engine/EngineTest.cpp
)

# Линкуем с нашей библиотекой и gtest
target_link_libraries(engine_test libcarbon gtest_main fmt)
target_include_directories(carbon_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/script)

if(MSVC)
    set_target_properties(engine_test PROPERTIES 
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/Debug"
    )
endif()

# Регистрируем тесты для Test Explorer
gtest_discover_tests(engine_test)


