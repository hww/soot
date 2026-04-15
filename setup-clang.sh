#!/bin/bash
# setup-clang.sh

PROJECT_ROOT=$(pwd)
BUILD_DIR="$PROJECT_ROOT/build"

echo "--- Настройка проекта Soot с использованием CLANG ---"

if [ -d "$BUILD_DIR" ]; then
    echo "Удаление старой папки build..."
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR" || exit

# Указываем пути к компиляторам
export CC=clang
export CXX=clang++

echo "Запуск CMake с Clang и libc++..."
# Добавляем флаги для использования libc++ и стандарта C++23
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-stdlib=libc++" \
      -DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++ -lc++abi" \
      -DCMAKE_CXX_STANDARD=23 \
      -DCMAKE_CXX_STANDARD_REQUIRED=ON \
      ..

echo "Сборка проекта..."
cmake --build . -j$(nproc)

echo "--- Готово! ---"