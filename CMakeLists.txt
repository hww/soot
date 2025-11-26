# Верхний уровень
cmake_minimum_required(VERSION 3.8)

project(repl
    VERSION 1.0.0
    DESCRIPTION "Z80 Lisp Interpreter"
    LANGUAGES C CXX
)

# C++ Modern CMake policies
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Build options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_SHARED_LIBS "Build as shared library" OFF)

# ВАЖНО: Отключаем тесты в fmt чтобы избежать конфликтов
set(FMT_TEST OFF CACHE BOOL "Disable fmt tests" FORCE)

# Version control will write to common/versions/revision.h file
include(CMakeLib.txt)
write_revision_h()


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
include(common/script/CMakeLists.txt)
include(common/repl/CMakeLists.txt)
include(common/type_system/CMakeLists.txt)
include(common/runtime/CMakeLists.txt)

# Installation (optional)
install(TARGETS lisp
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(TARGETS repl
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

#### ТЕСТЫ - ПРОСТОЙ ВАРИАНТ
if(BUILD_TESTS)
    # Отключаем лишнее в googletest
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    
    if(WIN32)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()
    
    # Добавляем googletest
    add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
    
    enable_testing()
    include(tests/script/CMakeLists.txt)
    include(tests/type_system/CMakeLists.txt)
    include(tests/vm/CMakeLists.txt)
endif()