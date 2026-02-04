# Верхний уровень
cmake_minimum_required(VERSION 3.10)

project(repl
    VERSION 1.0.0
    DESCRIPTION "SOOT: Scriptable Object-Oriented Toolkit"
    LANGUAGES C CXX
)

# C++ Modern CMake policies
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)
# или
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-pedantic)
endif()

# С CMake policies
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)  

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Compiler-agnostic configuration
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  
# Platform-specific settings
if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    # Windows 7+ compatibility
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION 7.0)
endif()

# Compiler flags
if(MSVC)
    # MSVC flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /utf-8 /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
    add_compile_options(/diagnostics:caret)  # Показывать конкретные строки
else()
    # GCC/Clang flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    
    # AddressSanitizer for debug builds
    option(USE_ASAN "Use Address Sanitizer" OFF)
    if(USE_ASAN)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    endif()
endif()

# --- Build options -------------------------------------------
# Флаг для сборки тестов (по умолчанию выключен)
option(BUILD_TESTS "Build tests" ON)

# ВАЖНО: Отключаем тесты в fmt чтобы избежать конфликтов
set(FMT_TEST OFF CACHE BOOL "Disable fmt tests" FORCE)

option(BUILD_SHARED_LIBS "Build as shared library" OFF)

# --- REVISIONS -----------------------------------------------
# Version control will write to common/versions/revision.h file
include(CMakeLib.txt)
write_revision_h()

# --- SUBPROJECTS DIRECTORIES ---------------------------------
# Include folders
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(common)
include_directories(common/cross_sockets)
include_directories(third_party/fmt/include)

# build format library
include_directories(third_party/fmt/include)
add_subdirectory(third_party/fmt EXCLUDE_FROM_ALL)

include_directories(third_party/replxx/include)
add_subdirectory(third_party/replxx EXCLUDE_FROM_ALL)

# Include subdirectories
include(common/sooti/CMakeLists.txt)
include(common/soot/CMakeLists.txt)
include(common/type_system/CMakeLists.txt)
include(common/carbon/CMakeLists.txt)
#include(common/carbonc/CMakeLists.txt)
include(common/xiff/CMakeLists.txt)
include(common/util/CMakeLists.txt)


# Если у тебя есть папка с дополнительными лиспами
# install(DIRECTORY common/lisp/ DESTINATION ${SOOT_SHARE_DIR}/lib)

# --- ТЕСТЫ -----------------------------------------------

if(BUILD_TESTS)
    # enable_testing должен быть вызван в корневом файле обязательно
    enable_testing()
    
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
    
    # Подключаем тесты
    include(tests/sooti/CMakeLists.txt)
    include(tests/type_system/CMakeLists.txt) # Убедитесь, что таргет там typesys_tests
    include(tests/carbon/CMakeLists.txt)
    include(tests/engine/CMakeLists.txt)
endif()

# Создать общую цель для сборки всех тестов
add_custom_target(tests
    COMMENT "Building all tests"
    DEPENDS 
        sooti_test
        typesys_tests  # ИСПРАВЛЕНО: было type_system_test
        carbon_test
        engine_test
)