# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(libtypesys 
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
target_link_libraries(libtypesys PRIVATE fmt::fmt)

# Для теста
add_executable(typesys common/type_system/Main.cpp)
target_link_libraries(typesys PRIVATE libtypesys fmt::fmt)
