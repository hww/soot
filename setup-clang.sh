#!/bin/bash
# setup-clang.sh

PROJECT_ROOT=$(pwd)
BUILD_DIR="$PROJECT_ROOT/build"

echo "--- Настройка проекта Soot с использованием CLANG ---"

# Очистка старой сборки
if [ -d "$BUILD_DIR" ]; then
    echo "Удаление старой папки build..."
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR" || exit

# Указываем Clang как системный компилятор для этой сессии
export CC=clang
export CXX=clang++

echo "Запуск CMake с Clang..."
cmake -DCMAKE_BUILD_TYPE=Debug ..

echo "Сборка проекта..."
cmake --build . -j$(nproc)

echo "--- Готово! Проверьте папку build/bin ---"