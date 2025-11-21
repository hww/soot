if (UNIX)
  set(CMAKE_CXX_FLAGS "-O3")
else ()
  set(CMAKE_CXX_FLAGS "/EHsc")
endif (UNIX)
set(CMAKE_CXX_STANDARD 20)

include_directories(../)

add_library(vm
assembler.cpp          
crc32.cpp        
module.cpp       
parser.cpp    
printer.cpp       
tokenizer.cpp  
vm_malloc.cpp
assembler_opcodes.cpp  
environment.cpp  
module_bin.cpp   
string_id.cpp     
variant.cpp    
native_func.cpp
context.cpp            
errors.cpp       
module_info.cpp  
platform.cpp  
syntax_tools.cpp  
vm.cpp
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(fmt PRIVATE FMT_EXPORT INTERFACE FMT_SHARED PUBLIC FMT_SHARED)
endif()

