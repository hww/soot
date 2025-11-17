# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(type_system_lib 
    common/type_system/type.cpp
    common/type_system/type_system.cpp
    common/type_system/type_spec.cpp
    common/type_system/defenum.cpp
    common/util/bit_utils.cpp
    common/util/string_util.cpp
)

# Линкуем fmt если нужно
target_link_libraries(type_system_lib PRIVATE fmt::fmt)

# Для теста
add_executable(type_system common/type_system/main.cpp)
target_link_libraries(type_system PRIVATE type_system_lib fmt::fmt)