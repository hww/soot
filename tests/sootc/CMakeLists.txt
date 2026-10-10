# Tests for the SOOTC compiler.
#
# These tests exercise the compiler end-to-end: source string -> Compiler
# -> BinaryFile -> VM execution. They are deliberately separate from
# tests/soot (which tests the interpreter) and tests/carbon (which tests
# the runtime pieces in isolation).

add_executable(sootc_test
    tests/sootc/TestCompilerBasics.cpp
    tests/sootc/TestCompilerTypes.cpp
    tests/sootc/TestCompilerNew.cpp
    tests/sootc/TestCompilerMethods.cpp
    tests/sootc/TestCompilerExpressions.cpp
    tests/sootc/TestCompilerIntegration.cpp
)

target_link_libraries(sootc_test PRIVATE
    libsootc
    libsot
    libsotcore
    libtypesys
    libsootutils
    fmt
    replxx
    xsockets_lib
    libcarbon
    gtest_main
)

if(WIN32)
    target_link_libraries(sootc_test PRIVATE ws2_32)
else()
    target_link_libraries(sootc_test PRIVATE pthread)
endif()

target_include_directories(sootc_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/common/sootc
    ${CMAKE_SOURCE_DIR}/tests/sootc
)

# The tests run from the project root, so relative paths like
# "soot_src/lib.soc" resolve correctly.
set_target_properties(sootc_test PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

gtest_discover_tests(sootc_test)