#!/bin/bash
# setup-gcc.sh

PROJECT_ROOT=$(pwd)
BUILD_DIR="$PROJECT_ROOT/build"

echo "--- Настройка проекта Soot с использованием GCC ---"

# Очистка старой сборки
if [ -d "$BUILD_DIR" ]; then
    echo "Удаление старой папки build..."
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR" || exit

# Указываем GCC как системный компилятор
export CC=gcc
export CXX=g++

echo "Запуск CMake с GCC..."
cmake -DCMAKE_BUILD_TYPE=Debug ..

echo "Сборка проекта..."
cmake --build . -j$(nproc)

echo "--- Готово! Проверьте папку build/bin ---"