

if (UNIX)
  set(CMAKE_CXX_FLAGS "-O3")
else ()
  set(CMAKE_CXX_FLAGS "/EHsc")
endif (UNIX)

include_directories(../)
add_executable(vm-asm
vm-asm.cpp
 )
target_link_libraries(vm-asm vm)

