# Верхний уровень
cmake_minimum_required(VERSION 3.10)

project(sootsystem
    VERSION 1.0.0
    DESCRIPTION "SOOT: Scriptable Object-Oriented Toolkit"
    LANGUAGES C CXX
)

# ============================================================
# C++ / C standard
# ============================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # GNU-расширения не нужны; clang-cl их не любит

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# Output dirs
# ============================================================
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ============================================================
# Platform-specific
# ============================================================
if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION 7.0)
endif()

# ============================================================
# Compiler flags
# ============================================================
if(MSVC)
    # MSVC и clang-cl — оба попадают сюда, потому что MSVC=true и для clang-cl.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /utf-8 /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
    add_compile_options(/diagnostics:caret)
else()
    # Только GCC/Clang на Linux/macOS.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wunused-function")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

    # libc++ — только для настоящего clang/gcc на Linux/macOS,
    # НЕ для clang-cl на Windows.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
    endif()

    option(USE_ASAN "Use Address Sanitizer" OFF)
    if(USE_ASAN)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    endif()

    # Debug-специфичные флаги для GCC/Clang (Linux/macOS).
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-O0 -g3 -ggdb -fno-omit-frame-pointer -fno-inline)
        add_compile_options(-fno-eliminate-unused-debug-types)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3 -ggdb")
    endif()
endif()

# ============================================================
# Helper: add subdirectory with reduced flags (only for Windows/clang hack)
# ============================================================
macro(safe_add_subdirectory dir)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
        set(_SAVED_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        set(_SAVED_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "-g -O0")
        set(CMAKE_C_FLAGS "-g -O0")
        add_subdirectory(${dir} EXCLUDE_FROM_ALL)
        set(CMAKE_CXX_FLAGS "${_SAVED_CMAKE_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "${_SAVED_CMAKE_C_FLAGS}")
    else()
        add_subdirectory(${dir} EXCLUDE_FROM_ALL)
    endif()
endmacro()

# ============================================================
# Build options
# ============================================================
option(BUILD_TESTS "Build tests" ON)
set(FMT_TEST OFF CACHE BOOL "Disable fmt tests" FORCE)
option(BUILD_SHARED_LIBS "Build as shared library" OFF)

# ============================================================
# Revisions
# ============================================================
include(CMakeLib.txt)
write_revision_h()

# ============================================================
# Subprojects
# ============================================================
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(common)
include_directories(third_party)

# fmt
safe_add_subdirectory(third_party/fmt)
include_directories(third_party/fmt/include)
include_directories(third_party/fmt)

# replxx
safe_add_subdirectory(third_party/replxx)
include_directories(third_party/replxx/include)
# replxx is a third-party library; silence its noisy warnings.
if(TARGET replxx)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(replxx PRIVATE
            -Wno-sign-conversion
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
            -Wno-padded
            -Wno-old-style-cast
        )
    elseif(MSVC)
        # MSVC doesn't have these warnings, but be safe:
        target_compile_options(replxx PRIVATE /wd4267 /wd4244)
    endif()
endif()

# main subdirectories
include(common/cross_sockets/CMakeLists.txt)
include(common/repl/CMakeLists.txt)
include(common/soot/CMakeLists.txt)
include(common/sootc/CMakeLists.txt)
include(common/type_system/CMakeLists.txt)
include(common/carbon/CMakeLists.txt)
include(common/xiff/CMakeLists.txt)
include(common/util/CMakeLists.txt)

set_target_properties(sootc PROPERTIES
    VS_DEBUGGER_COMMAND_ARGUMENTS   "--script test.sot"
)

# ============================================================
# Tests
# ============================================================
if(BUILD_TESTS)
    enable_testing()
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
endif()

# Собираем общую цель tests только если её цели реально определены.
# Иначе add_custom_target упадёт при конфигурации (это и было в WSL).
if(TARGET sooti_test AND TARGET typesys_tests AND TARGET carbon_test AND TARGET engine_test)
    add_custom_target(tests
        COMMENT "Building all tests"
        DEPENDS sooti_test typesys_tests carbon_test engine_test
    )
endif()