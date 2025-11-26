# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(type_system_lib 
    common/util/BitUtils.cpp
    common/util/StringUtil.cpp
    common/util/Diff.cpp
    common/util/FileUtil.cpp
    common/util/Log.cpp
    
    common/type_system/Type.cpp
    common/type_system/TypeSystem.cpp
    common/type_system/TypeSpec.cpp
    common/type_system/Defenum.cpp
    common/type_system/Deftype.cpp
    common/type_system/State.cpp
)

# Линкуем fmt если нужно
target_link_libraries(type_system_lib PRIVATE fmt::fmt)

# Для теста
add_executable(type_system common/type_system/main.cpp)
target_link_libraries(type_system PRIVATE type_system_lib fmt::fmt)