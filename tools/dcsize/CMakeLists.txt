# ---------------------------------------------------------------------------
# dcsize: print sizes of all DC-format structures
# ---------------------------------------------------------------------------
# Verifies that struct layouts match the expected byte sizes.

add_executable(dcsize
    tools/dcsize/dcsize.cpp
)

target_link_libraries(dcsize PRIVATE
    libcarbon
    fmt
)

set_target_properties(dcsize PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)