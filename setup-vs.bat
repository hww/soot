@echo off
REM setup-vs.bat
REM Настройка проекта Soot для Visual Studio с использованием LLVM/Clang

set PROJECT_ROOT=%cd%
set BUILD_DIR=%PROJECT_ROOT%\build_vs

echo --- Настройка проекта Soot для Visual Studio (Clang-CL) ---

if exist "%BUILD_DIR%" (
    echo Удаление старой папки build_vs...
    rmdir /s /q "%BUILD_DIR%"
)

mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%" || exit /b

REM Генерируем проект для Visual Studio 2022 с тулсетом ClangCL
echo Запуск CMake...
cmake -G "Visual Studio 17 2022" -A x64 -T ClangCL ^
      -DCMAKE_BUILD_TYPE=Debug ^
      -DCMAKE_CXX_STANDARD=23 ^
      -DCMAKE_CXX_STANDARD_REQUIRED=ON ^
      ..

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] CMake failed. Убедитесь, что компонент "C++ Clang-cl" установлен в VS Installer.
    pause
    exit /b %errorlevel%
)

echo.
echo --- Готово! ---
echo Проект сконфигурирован под LLVM (clang-cl).
echo Откройте решение: %BUILD_DIR%\soot.sln
pause