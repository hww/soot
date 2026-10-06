# ---------------------------------------------------------------------------
# sidbase: string-id database generator
# ---------------------------------------------------------------------------
# Scans C++ source files for SID("...") occurrences and produces a database
# of string IDs. Can also merge existing databases.

add_executable(sidbase
    tools/sidbase/sidbase.cpp
)

target_link_libraries(sidbase PRIVATE
    libcarbon
    fmt
)

if(WIN32)
    target_link_libraries(sidbase PRIVATE ws2_32)
else()
    target_link_libraries(sidbase PRIVATE pthread)
endif()

set_target_properties(sidbase PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)