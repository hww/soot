# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(libtypesys 
    common/type_system/Type.cpp
    common/type_system/TypeSystem.cpp
    common/type_system/TypeSpec.cpp
    common/type_system/Defenum.cpp
    common/type_system/Deftype.cpp
    common/type_system/State.cpp
    common/type_system/Register.cpp

)

# Линкуем fmt если нужно
target_link_libraries(libtypesys PRIVATE libsoticore fmt::fmt libsootutils)

# Для теста
add_executable(typesys common/type_system/Main.cpp)
target_link_libraries(typesys PRIVATE libtypesys fmt::fmt)
