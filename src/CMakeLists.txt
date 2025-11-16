function(write_revision_h)
    find_package(Git)
    set(GIT_SHORT_SHA "")
    set(GIT_TAG "")
    if(NOT GIT_FOUND)
      MESSAGE(STATUS "write_revision_h(): git was not found")
    endif()
    MESSAGE(STATUS "write_revision_h(): ${CMAKE_SOURCE_DIR}")
    if (GIT_FOUND AND EXISTS ${CMAKE_SOURCE_DIR}/.git)
        MESSAGE(STATUS "Git found, using it to get revision info")
        EXECUTE_PROCESS(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                OUTPUT_VARIABLE GIT_SHORT_SHA
                OUTPUT_STRIP_TRAILING_WHITESPACE)
        EXECUTE_PROCESS(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} tag --points-at HEAD
                OUTPUT_VARIABLE GIT_TAG
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
        # To support being on a commit with mutiple tags
        if(${GIT_TAG} MATCHES "\n")
          string(REPLACE "\n" ";" GIT_TAG_LIST "${GIT_TAG}")
          list(LENGTH GIT_TAG_LIST GIT_TAG_LIST_LENGTH)
          math(EXPR GIT_TAG_LAST_INDEX "${GIT_TAG_LIST_LENGTH} - 1")
          list(GET GIT_TAG_LIST ${GIT_TAG_LAST_INDEX} GIT_TAG)
        endif()
    endif()
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/versions/revision.h "#define BUILT_TAG \"${GIT_TAG}\"\n#define BUILT_SHA \"${GIT_SHORT_SHA}\"\n")
endfunction()

write_revision_h()

include_directories(third_party/fmt/include)
include_directories(src)

# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(script_lib 
    src/script/object.cpp
    src/script/reader.cpp
    src/script/interpreter.cpp
    src/script/source_info.cpp
    src/script/crc32.cpp
    src/script/pretty_printer.cpp

)

add_executable(repl src/repl/main.cpp)
target_link_libraries(repl PRIVATE script_lib  fmt replxx)

# Исполняемый файл
add_executable(lisp src/script/main.cpp)
target_link_libraries(lisp PRIVATE script_lib  fmt)