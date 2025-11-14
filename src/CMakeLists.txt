# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(z80-lisp-lib
    lib/object.cpp
    lib/reader.cpp
    lib/interpreter.cpp
    lib/source-info.cpp
)

# Линкуем с replxx
target_link_libraries(z80-lisp-lib PRIVATE)

# Исполняемый файл
add_executable(z80-lisp main.cpp)
target_link_libraries(z80-lisp PRIVATE z80-lisp-lib)