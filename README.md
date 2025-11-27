# TODO: Z80-Lisp Virtual Machine

# Aleste LX Virtual Machine

![C++](https://img.shields.io/badge/C++-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-064F8C?style=flat&logo=cmake&logoColor=white)
![Virtual Machine](https://img.shields.io/badge/Virtual_Machine-FF6B6B?style=flat&logo=virtualbox&logoColor=white)
![Compiler](https://img.shields.io/badge/Compiler-4ECDC4?style=flat&logo=gnu&logoColor=white)
![Cross Platform](https://img.shields.io/badge/Cross_Platform-8E44AD?style=flat&logo=windows&logoColor=white)
![Retro Computing](https://img.shields.io/badge/Retro_Computing-FF9A00?style=flat&logo=retroarch&logoColor=white)
![FPGA](https://img.shields.io/badge/FPGA-7B1FA2?style=flat&logo=intel&logoColor=white)
![Status](https://img.shields.io/badge/Status-Active-success)

![REPL Screenshot](/docs/screens/repl.png)

## Текущее состояние ✅
**ЗАВЕРШЕНО И РАБОТАЕТ:**
- Интерпретатор байткода (VirtualMachine)
- Система типов (Scalar,Enums,Structures,Bitfields,...,Methods) 
- BinaryFile формат и загрузка байткода и статической области
- Система модулей, разрешение зависимостей и линковка
- Native functions для вызова методов C++
- Process, Sheduler, State система процессов и конечных сочтояний
- Engine, Connection система сложных взаимодействий между процессами
- Базовые тесты исполнения

## Приоритет 1: Загрузка и линковка модулей 🔴

### 1.1. Глубже тестировать систему модулей
- [ ] **ModuleRegistry** - глобальный реестр модулей нужно больше тестов
- [ ] **Динамическая загрузка** - загрузка модулей в runtime тесты загрузки
- [ ] **Линковка** - разрешение cross-module ссылок 
- [ ] **Импорт/экспорт** - таблицы импорта/экспорта между модулями
- [ ] **Зависимости** - управление зависимостями модулей

## Приоритет 2: Система процессов и планировщик 🔴

### 2.1. Более глубокое тестирование (Kernel)
- [ ] **Process management** - создание/удаление процессов
- [ ] **Scheduler** - планировщик с приоритетами
- [ ] **Event system** - система событий между процессами
- [ ] **Message passing** - межпроцессное взаимодействие
- [ ] **Error recovery** - восстановление после ошибок в процессах

## Приоритет 3: Конечные автоматы (State Machines) 🔴

### 3.1. StateDefinition система
- [ ] **State transitions** - переходы между состояниями
- [ ] **Virtual methods** - виртуальные методы состояний
- [ ] **Hierarchical states** - иерархические состояния
- [ ] **State history** - история состояний

## Приоритет 4: Расширенные фреймы 🟡

### 4.1. Тестирование специализированных фреймов
- [ ] **Exception frames** - обработка исключений
- [ ] **Catch frames** - блоки catch
- [ ] **Generator frames** - генераторы и корутины
- [ ] **Async frames** - асинхронное выполнение

## Дополнительные улучшения 🟢

### Инструменты разработки
- [ ] **Debugger** - отладчик байткода
- [ ] **Profiler** - профилирование выполнения
- [ ] **Memory analyzer** - анализ использования памяти

### Оптимизации
- [ ] **JIT compilation** - компиляция в native code
- [ ] **Bytecode optimization** - оптимизация байткода
- [ ] **Garbage collection** - сборка мусора
