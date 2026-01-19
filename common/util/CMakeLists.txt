# Создаем нашу библиотеку БЕЗ replxx файлов
add_library(libsootutils 

common/util/Assert.cpp 
common/util/BitUtils.cpp 
common/util/Crc32.cpp 
common/util/Diff.cpp 
common/util/FileUtil.cpp 
common/util/ImageResize.cpp 
common/util/JsonUtil.cpp 
common/util/Log.cpp 
common/util/PrintFloat.cpp 
common/util/StringUtil.cpp 
common/util/UnicodeUtil.cpp 
common/util/FileUtil.cpp
common/util/PrintFloat.cpp
common/util/Diff.cpp  # или другой файл с реализацией google_diff
)

# Линкуем fmt если нужно
target_link_libraries(libsootutils PRIVATE fmt::fmt)
