# ---------------------------------------------------------------------------
# dcinspect: DC file entry-table dumper
# ---------------------------------------------------------------------------
# Opens a DC file, validates it, and prints its entry table.

add_executable(dcinspect
    tools/dcinspect/dcinspect.cpp
)

target_link_libraries(dcinspect PRIVATE
    libcarbon
    fmt
)

if(WIN32)
    target_link_libraries(dcinspect PRIVATE ws2_32)
else()
    target_link_libraries(dcinspect PRIVATE pthread)
endif()

set_target_properties(dcinspect PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)