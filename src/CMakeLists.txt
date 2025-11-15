# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(z80-lisp-lib
    src/lib/object.cpp
    src/lib/reader.cpp
    src/lib/interpreter.cpp
    src/lib/source-info.cpp
    src/lib/crc32.cpp
)

# Линкуем с replxx
target_link_libraries(z80-lisp-lib PRIVATE)

# Исполняемый файл
add_executable(z80-lisp src/main.cpp)
target_link_libraries(z80-lisp PRIVATE z80-lisp-lib fmt)