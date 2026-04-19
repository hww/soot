@echo off
REM setup-vs.bat
REM Настройка проекта Soot для Visual Studio

set PROJECT_ROOT=%cd%
set BUILD_DIR=%PROJECT_ROOT%\build_vs

echo --- Настройка проекта Soot для Visual Studio ---

if exist "%BUILD_DIR%" (
    echo Удаление старой папки build_vs...
    rmdir /s /q "%BUILD_DIR%"
)

mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%" || exit /b

REM Генерируем проект для Visual Studio 2022
echo Запуск CMake с генератором для Visual Studio 2022...
cmake -G "Visual Studio 17 2022" -A x64 ^
      -DCMAKE_BUILD_TYPE=Debug ^
      -DCMAKE_CXX_STANDARD=23 ^
      -DCMAKE_CXX_STANDARD_REQUIRED=ON ^
      ..

echo.
echo --- Готово! ---
echo Откройте решение: %BUILD_DIR%\soot.sln
echo.
echo Для компиляции в VS Code используйте задачи:
echo - Ctrl+Shift+B - сборка
echo - Ctrl+F5 - запуск без отладки
echo - F5 - запуск с отладкой