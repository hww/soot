
# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(vm_lib 
    common/util/crc32.cpp
    common/util/assert.cpp
    common/util/file_util.cpp
    common/util/print_float.cpp
    common/util/log.cpp
    common/vm/native_func.cpp
    common/vm/string_id.cpp
    
)

