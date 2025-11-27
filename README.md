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
- Система типов (Variant) 
- Управление памятью (Ptr)
- BinaryFile формат и загрузка
- Система модулей (Module)
- Native functions
- Базовые тесты исполнения

## Приоритет 1: Загрузка и линковка модулей 🔴

### 1.1. Завершить систему модулей
- [ ] **ModuleRegistry** - глобальный реестр модулей
- [ ] **Динамическая загрузка** - загрузка модулей в runtime
- [ ] **Линковка** - разрешение cross-module ссылок
- [ ] **Импорт/экспорт** - таблицы импорта/экспорта между модулями
- [ ] **Зависимости** - управление зависимостями модулей

### 1.2. Тестирование загрузки
- [ ] **Мульти-модульные тесты** - взаимодействие нескольких модулей
- [ ] **Circular dependencies** - обработка циклических зависимостей
- [ ] **Hot reload** - перезагрузка модулей без остановки VM
- [ ] **Memory management** - очистка загруженных модулей

## Приоритет 2: Система процессов и планировщик 🔴

### 2.1. Завершить ядро (Kernel)
- [ ] **Process management** - создание/удаление процессов
- [ ] **Scheduler** - планировщик с приоритетами
- [ ] **Event system** - система событий между процессами
- [ ] **Message passing** - межпроцессное взаимодействие

### 2.2. Тестирование верхнего уровня
- [ ] **Process isolation** - изоляция процессов
- [ ] **Scheduler stress tests** - нагрузочное тестирование
- [ ] **Event handling** - обработка событий и сообщений
- [ ] **Error recovery** - восстановление после ошибок в процессах

### 2.3. Маски и привилегии
- [ ] **Security masks** - маски доступа для процессов
- [ ] **Privilege levels** - уровни привилегий
- [ ] **Resource quotas** - квоты ресурсов на процесс

## Приоритет 3: Конечные автоматы (State Machines) 🔴

### 3.1. StateDefinition система
- [ ] **State transitions** - переходы между состояниями
- [ ] **Virtual methods** - виртуальные методы состояний
- [ ] **Hierarchical states** - иерархические состояния
- [ ] **State history** - история состояний

### 3.2. Интеграция с процессами
- [ ] **Process states** - состояния процессов как state machines
- [ ] **State persistence** - сохранение/восстановление состояний
- [ ] **State validation** - валидация переходов между состояниями

## Приоритет 4: Расширенные фреймы 🟡

### 4.1. Специализированные фреймы
- [ ] **Exception frames** - обработка исключений
- [ ] **Catch frames** - блоки catch
- [ ] **Generator frames** - генераторы и корутины
- [ ] **Async frames** - асинхронное выполнение

### 4.2. Тестирование фреймов
- [ ] **Exception propagation** - распространение исключений через фреймы
- [ ] **Stack unwinding** - раскрутка стека при исключениях
- [ ] **Frame introspection** - интроспекция фреймов для отладки

## Дополнительные улучшения 🟢

### Инструменты разработки
- [ ] **Debugger** - отладчик байткода
- [ ] **Profiler** - профилирование выполнения
- [ ] **Memory analyzer** - анализ использования памяти

### Оптимизации
- [ ] **JIT compilation** - компиляция в native code
- [ ] **Bytecode optimization** - оптимизация байткода
- [ ] **Garbage collection** - сборка мусора

---

## Начало работы после перерыва 🚀

**Рекомендуемый порядок:**
1. Начать с **Приоритета 1** (модули) - это фундамент
2. Перейти к **Приоритету 2** (процессы) - интеграция
3. Затем **Приоритет 3** (автоматы) - бизнес-логика  
4. И наконец **Приоритет 4** (фреймы) - расширенные возможности

**Перед началом:**
- Запустить существующие тесты: `./run_tests.sh`
- Проверить базовую функциональность: `./samples/hello_world.lisp`
- Просмотреть логи последней стабильной сборки

---
*Последнее обновление: $(date)*  
*Текущая ветка: development*  
*Статус: Пауза - переход на FPGA проекты*
