#!/bin/bash
# setup-gcc.sh

PROJECT_ROOT=$(pwd)
BUILD_DIR="$PROJECT_ROOT/build"

echo "--- Настройка проекта Soot с использованием GCC ---"

rm -rf "$BUILD_DIR"
mkdir "$BUILD_DIR"
cd "$BUILD_DIR" || exit

export CC=gcc
export CXX=g++

# Добавляем генерацию compile_commands.json для LSP
echo "Запуск CMake..."
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DCMAKE_CXX_STANDARD=23 \
      ..

echo "Сборка проекта..."
cmake --build . -j$(nproc)