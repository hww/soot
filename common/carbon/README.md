# Анализ текущего состояния системы

Ваша система состоит из трех основных компонентов:

1. **Ассемблер** - почти завершен, с хорошей системой парсинга и кодогенерации
2. **Виртуальная машина** - частично реализована, требует доработки
3. **Бинарный формат** - хорошо проработан, с поддержкой модулей и определений

## Спецификация ассемблерного языка VM

### Формат инструкций

```cpp
struct FInstr {
    EOpcode opcode;  // 8 бит
    u8 a;           // регистр назначения
    u8 b;           // исходный регистр 1  
    u8 c;           // исходный регистр 2 / младшие 8 бит immediate
    u16 k;          // 16-битное immediate значение (объединение b и c)
};
```

### Система команд (группировка по типам операций)

#### Управление выполнением

- `ret a` - возврат из функции
- `call a, b, k` - вызов функции (a-функция, b-регистр результата, k-кол-во аргументов)
- `calln a, b, k` - вызов нативной функции
- `br k` - безусловный переход
- `brif a, k` - условный переход если true
- `brno a, k` - условный переход если false

#### Целочисленные операции

```lisp
(add r0, r1, r2)    ; r0 = r1 + r2
(sub r0, r1, r2)    ; r0 = r1 - r2  
(mul r0, r1, r2)    ; r0 = r1 * r2
(div r0, r1, r2)    ; r0 = r1 / r2
(mod r0, r1, r2)    ; r0 = r1 % r2
(abs r0, r1)        ; r0 = |r1|
(neg r0, r1)        ; r0 = -r1
(ash r0, r1)        ; r0 = r1 << r2 (арифметический сдвиг)
```

#### Операции с плавающей точкой

```lisp  
(fadd r0, r1, r2)   ; r0 = r1 + r2
(fsub r0, r1, r2)   ; r0 = r1 - r2
(fmul r0, r1, r2)   ; r0 = r1 * r2  
(fdiv r0, r1, r2)   ; r0 = r1 / r2
(fmod r0, r1, r2)   ; r0 = fmod(r1, r2)
(fabs r0, r1)       ; r0 = fabs(r1)
(fneg r0, r1)       ; r0 = -r1
```

#### Загрузка констант

```lisp
(ldk r0, 123)       ; загрузка immediate
(ldsi r0, data[5])  ; загрузка статического целого
(ldsf r0, data[2])  ; загрузка статического float  
(ldsp r0, data[8])  ; загрузка статического указателя
```

#### Сравнения

```lisp
(cpeq r0, r1, r2)   ; целочисленное сравнение =
(cpgt r0, r1, r2)   ; целочисленное сравнение >
(fcpeq r0, r1, r2)  ; float сравнение =  
(fcpgt r0, r1, r2)  ; float сравнение >
```

### Директивы ассемблера

#### Определение функций

```lisp
(define (function-name arg1 arg2)
  (let ((local-var1 : type) 
        (local-var2 default-value : type))
    ; тело функции
    (add r0, r1, r2)
    (ret r0)))
```

#### Метки и данные

```lisp
(label function-start)
(define *global-var* 42)
(define *player-pos* (new vec4 :x 0 :y 0 :z 0))
```

## Спецификация бинарного формата

### Заголовок файла

```cpp
struct FBinFileHeader {
    u32 magic_num = 0x00435844; // "DC00"
    u32 file_size;              // полный размер файла
    u32 used_size;              // использованный размер
    u32 defs_max;               // максимальное число определений
    u32 defs_num;               // текущее число определений  
    u32 defs_offs;              // смещение таблицы определений
    PTRINT offset;              // базовое смещение
};
```

### Структура определений

```cpp
struct FDefinition {
    StringId Name;    // имя определения (SID)
    StringId Type;    // тип ("lambda", "s32", "float", etc.)
    u32 Offset;       // смещение от начала файла
};
```

### Организация байткода

```cpp
struct FFunctionDesc {
    u32 desc_size;    // полный размер (заголовок + код + данные)
    u32 file_offs;    // смещение от начала файла
    u32 code_offs;    // смещение кода
    u32 data_offs;    // смещение данных
    
    FInstr* get_code_ptr() { return (FInstr*)((PTRINT)this + (code_offs - file_offs)); }
    FRecord* get_data_ptr() { return (FRecord*)((PTRINT)this + (data_offs - file_offs)); }
};
```

## Спецификация виртуальной машины

### Организация памяти выполнения

#### Регистры (34 регистра)

```cpp
constexpr size_t DC_FRAME_MAX_REGISTERS_NUM = 34;
constexpr size_t ARGUMENT_REGISTERS_OFFSET = 24;  // r24-r33: аргументы
constexpr size_t LOCAL_REGISTERS_OFFSET = 0;      // r0-r23: локальные переменные
```

#### Структура фрейма

```cpp
struct FStackFrame {
    FInstr* code_ptr;      // указатель на код
    FRecord* data_ptr;     // указатель на данные
    FStackFrame* parent_ptr; // родительский фрейм
    size_t pc;             // программный счетчик
    s32 argc;              // количество аргументов
    s32 ret_num;           // регистр возврата
    FVariant registers[DC_FRAME_MAX_REGISTERS_NUM]; // регистры
};
```

### Типы данных Variant

```cpp
struct FVariant {
    StringId type;  // SID("nil"), SID("s32"), SID("f32"), SID("ptr")
    union {
        PTRINT as_ptr;
        s32 as_s32; 
        f32 as_f32;
    };
};
```

## Интеграционные точки с интерпретатором лиспа

### 1. Использование систематипов

Ваш интерпретатор может генерировать AST, которое затем преобразуется в:

- **Структуры данных** → таблица данных в `.bin` файле
- **Функции/лямбды** → байткод VM  
- **Определения типов** → информация о типах для runtime

### 2. Процесс компиляции

```text
Lisp AST 
    → (ваш интерпретатор) 
    → промежуточное представление 
    → (существующий ассемблер) 
    → бинарный файл
```

### 3. Совместимость типов

Система типов вашего интерпретатора должна маппиться на:

- `integer` → `SID("s32")`
- `float` → `SID("f32")`
- `string` → `SID("string")`
- `symbol` → `SID("symbol")`
- `lambda` → `SID("lambda")`

## Рекомендации по улучшению VM

### 1. Управление памятью

```cpp
// Добавить в FStackFrame
void initialize_locals(); // инициализация локальных переменных
void cleanup_locals();    // очистка (для GC)
```

### 2. Система исключений

```cpp
struct FVMException {
    EExceptionType type;
    StringId message;
    size_t pc;
    FStackFrame* frame;
};
```

### 3. Отладка

```cpp
struct FDebugInfo {
    std::map<size_t, Location> pc_to_source; // mapping PC → исходный код
    std::vector<StringId> local_names;       // имена локальных переменных
};
```

Отличные вопросы! Разберем все по порядку.

## Процесс интерпретации: Создание и удаление фреймов

### Создание фрейма

```cpp
FStackFrame* PushStackFrame(FFunctionDesc* pCode, FStackFrame* pParent) {
    auto* pFrame = new FStackFrame();
    pFrame->parent_ptr = pParent;
    pFrame->code_ptr = pCode->get_code_ptr();
    pFrame->data_ptr = pCode->get_data_ptr();
    pFrame->pc = 0;  // начинаем с первой инструкции
    pFrame->argc = 0;
    pFrame->ret_num = 0;
    
    // Инициализация регистров
    for (auto& reg : pFrame->registers) {
        reg.set_as_null();
    }
    return pFrame;
}
```

**Пример вызова:**

```lisp
; Исходный код
(define (main)
  (let ((x 10))
    (add-one x)))

(define (add-one num)
  (+ num 1))

; Байткод вызова
(call add-one-func, r25, 1)  ; вызов функции, результат в r25, 1 аргумент
```

**Процесс создания фрейма:**

1. `main` выполняется в текущем фрейме
2. При `call` создается новый фрейм для `add-one`
3. Аргументы копируются из регистров `r24+` вызывающего фрейма в `r24+` нового фрейма

### Удаление фрейма

```cpp
FStackFrame* PopStackFrame(FStackFrame* pFrame) {
    // Возвращаем результат в родительский фрейм
    if (pFrame->parent_ptr) {
        FVariant result = pFrame->registers[pFrame->ret_num];
        pFrame->parent_ptr->registers[pFrame->ret_num] = result;
    }
    
    // Очищаем сложные объекты (если есть GC)
    cleanup_frame_objects(pFrame);
    
    FStackFrame* parent = pFrame->parent_ptr;
    delete pFrame;
    return parent;
}
```

## Локальные переменные vs Аргументы

### Схема распределения регистров

```text
Регистры 0-23: Локальные переменные
Регистры 24-33: Аргументы функции
```

**Пример:**

```lisp
(define (calculate a b)
  (let ((local1 (+ a b))
        (local2 (* a b)))
    (+ local1 local2)))

; Распределение регистров:
; r24 = a (аргумент 1)
; r25 = b (аргумент 2)  
; r0 = local1 (локальная 1)
; r1 = local2 (локальная 2)
```

**Байткод:**

```lisp
; let ((local1 (+ a b)))
(add r0, r24, r25)     ; r0 = r24 + r25

; let ((local2 (* a b)))  
(mul r1, r24, r25)     ; r1 = r24 * r25

; (+ local1 local2)
(add r25, r0, r1)      ; результат в r25
(ret r25)              ; возврат
```

## Адресация в parent frame

### Механизм замыканий и доступа к внешним переменным

```cpp
// В вашем коде есть foundation для этого:
FVariant& FStackFrame::get_register(u32 index) {
    // TODO: Добавить поддержку parent frame lookup
    // Сейчас работает только с локальными регистрами
    assert(index < DC_FRAME_MAX_REGISTERS_NUM);
    return registers[index];
}

// Предлагаемое улучшение:
FVariant& FStackFrame::resolve_register(u32 index) {
    if (index < DC_FRAME_MAX_REGISTERS_NUM) {
        return registers[index];
    } else if (parent_ptr) {
        // Индекс указывает на родительский фрейм
        return parent_ptr->resolve_register(index - DC_FRAME_MAX_REGISTERS_NUM);
    }
    throw FRuntimeError("Register index out of bounds");
}
```

**Пример замыкания:**

```lisp
(define (make-counter initial)
  (lambda ()
    (let ((count initial))
      (set! count (+ count 1))
      count)))

; Реализация через parent frame доступ:
; Внутренняя lambda имеет доступ к 'initial' из родительского фрейма
```

## Работа со сложными структурами

### 1. Массивы (Array)

**Представление в памяти:**

```cpp
struct FArray {
    u32 length;
    u32 capacity;
    FVariant elements[1];  // flexible array member
};
```

**Передача между методами:**

```lisp
(define (process-array arr)
  (let ((len (array-length arr)))
    (array-push arr 42)  ; модификация исходного массива
    len))

; Байткод:
(lookp r24, "arr")        ; загрузка указателя на массив
(ldri r25, r24)           ; загрузка длины массива (r25 = arr->length)
; array-push реализован как нативная функция
(calln array-push-native, r26, 2)  ; arr и 42 как аргументы
```

### 2. Строки (String)

**Представление:**

```cpp
struct FString {
    u32 length;
    u32 hash;           ; для быстрого сравнения
    char data[1];       ; UTF-8 данные
};
```

**Операции:**

```lisp
(define (concatenate str1 str2)
  (string-append str1 str2))

; Байткод - обычно как нативный вызов:
(lookp r24, "str1")
(lookp r25, "str2")  
(calln string-append-native, r26, 2)  ; результат в r26
```

### 3. Пользовательские структуры

**Определение типа:**

```lisp
(deftype vec3 
  ((x float)
   (y float) 
   (z float)))

(define (normalize vec)
  (let ((len (sqrt (+ (* (-> vec x) (-> vec x))
                      (* (-> vec y) (-> vec y))
                      (* (-> vec z) (-> vec z))))))
    (new vec3 
         :x (/ (-> vec x) len)
         :y (/ (-> vec y) len) 
         :z (/ (-> vec z) len))))
```

**Доступ к полям:**

```cpp
// Компилятор знает смещения полей:
// vec3::x = offset 0, vec3::y = offset 4, vec3::z = offset 8

; Байткод доступа к полю:
(ldsp r0, "vec")           ; загрузка указателя на структуру
(ldrf r1, r0)              ; r1 = vec->x (смещение 0)
(ldrf r2, r0+4)            ; r2 = vec->y (смещение 4)  
(ldrf r3, r0+8)            ; r3 = vec->z (смещение 8)
```

## Управление памятью - анализ вашей системы

### Сильные стороны

1. **Variant система** - хорошая основа:

```cpp
struct FVariant {
    StringId type;
    union {
        PTRINT as_ptr;  // для сложных объектов
        s32 as_s32;     // встроенные типы
        f32 as_f32;
    };
};
```

1. **Разделение данных и кода** в `FFunctionDesc`

2. **Система определений** для глобального доступа

### Проблемы и улучшения

#### 1. Отсутствие сборки мусора

**Текущее состояние:** Утечки памяти при создании объектов.

**Решение - добавить подсчет ссылок:**

```cpp
struct FGCObject {
    std::atomic<u32> ref_count;
    StringId type;
    
    void grab() { ref_count++; }
    void release() { 
        if (--ref_count == 0) {
            destroy_object(this);
        }
    }
};

struct FArray : FGCObject {
    u32 length;
    FVariant elements[1];
};
```

#### 2. Улучшение системы регистров

```cpp
struct FStackFrame {
    // Текущая проблема: регистры хранят сырые указатели
    FVariant registers[DC_FRAME_MAX_REGISTERS_NUM];
    
    // Решение: автоматическое управление ссылками
    void set_register(u32 index, const FVariant& value) {
        // Уменьшить счетчик старого значения
        if (registers[index].is_ptr()) {
            dereference_object(registers[index].as_ptr);
        }
        
        // Увеличить счетчик нового значения  
        if (value.is_ptr()) {
            grab_object(value.as_ptr);
        }
        
        registers[index] = value;
    }
};
```

#### 3. Управление жизненным циклом фреймов

```cpp
class FStackFrame {
private:
    std::vector<FGCObject*> local_objects;  // для автоматического release
    
public:
    ~FStackFrame() {
        // Автоматическая очистка при разрушении фрейма
        for (auto obj : local_objects) {
            obj->release();
        }
        for (auto& reg : registers) {
            if (reg.is_ptr()) {
                dereference_object(reg.as_ptr);
            }
        }
    }
    
    void track_object(FGCObject* obj) {
        local_objects.push_back(obj);
        obj->grab();
    }
};
```

### Пример полного жизненного цикла

```lisp
(define (create-and-process)
  (let ((arr (array-new 10))     ; создание массива - refcount = 1
        (result 0))
    (array-fill arr 42)          ; использование массива
    (set! result (process-array arr))  ; передача в другую функцию
    result))                     ; массив автоматически уничтожается

(define (process-array arr)
  (array-push arr 100)           ; refcount временно увеличивается
  (array-length arr))            ; возвращает примитив - нет утечки
```

**Байткод с управлением памятью:**

```lisp
; create-and-process:
(calln array-new-native, r0, 1)  ; r0 = новый массив (refcount=1)
(track r0)                       ; фрейм начинает отслеживать объект
(move r24, r0)                   ; подготовка аргумента
(call process-array-func, r1, 1) ; вызов - временное увеличение refcount
; при выходе из фрейма - автоматический release r0
```

# Система управления памятью ScriptObject

## 📋 Обзор

ScriptObject - это система автоматического управления памятью для встраиваемых скриптовых систем, использующая reference counting с нулевым оверхедом для доступа к данным из скриптов.

## 🎯 Основная идея

Скрипты работают с чистыми указателями на данные, в то время как VM невидимо управляет временем жизни объектов через подсчет ссылок, используя смещение указателей.

## 🏗️ Архитектура

### Структура памяти

```text
[ref_count:4][данные...]
^            ^
base_ptr     data_ptr
```

- **Скрипты видят**: `data_ptr` - чистый указатель на данные
- **VM управляет через**: `base_ptr = data_ptr - 4` - полный объект с refcount

### Базовые структуры

```cpp
struct ScriptObjectBase {
    int32_t ref_count;  // 4 байта - счетчик ссылок
};

template<typename T>
struct ScriptObject : ScriptObjectBase {
    T data;  // Данные начинаются сразу после ref_count
};
```

## 📝 API системы

### Создание объектов

```cpp
// Создание объекта любого типа
Vector3* vec = script_create<Vector3>(1.0f, 2.0f, 3.0f);
std::string* str = script_create<std::string>("hello");
MyClass* obj = script_create<MyClass>(arg1, arg2);
```

### Управление ссылками

```cpp
// Увеличение счетчика
script_ref(data_ptr);

// Уменьшение счетчика (автоматическое удаление при 0)
script_unref(data_ptr);

// Получение текущего счетчика
int32_t count = script_ref_count(data_ptr);
```

### Вспомогательные функции

```cpp
// Получение base pointer из data pointer
ScriptObjectBase* base = to_script_base(data_ptr);

// Типизированный доступ к обертке
ScriptObject<Vector3>* wrapper = script_get_wrapper(vec_ptr);
```

## 🔄 Интеграция с Variant

```cpp
class Variant {
public:
    // Автоматическое управление в деструкторе
    ~Variant() {
        if (is_ptr()) script_unref(ptr_value);
    }
    
    // Автоматическое управление при присваивании
    void set_ptr(void* ptr, StringId type) {
        if (is_ptr()) script_unref(ptr_value);  // Старый объект
        ptr_value = ptr;
        if (ptr) script_ref(ptr);              // Новый объект
    }
};
```

## 🎪 "Магический трюк"

### Ключевое преобразование

```cpp
// Из data_ptr в base_ptr
ScriptObjectBase* to_script_base(const void* data_ptr) {
    return reinterpret_cast<ScriptObjectBase*>(
        reinterpret_cast<const uint8_t*>(data_ptr) - sizeof(ScriptObjectBase)
    );
}

// Обратно - из base_ptr в data_ptr
T* get_data_ptr() { return &this->data; }
```

### Преимущества подхода

- ✅ **Нулевой оверхед** в скриптах - прямые доступы к памяти
- ✅ **Автоматическое управление** - скрипты не заботятся о памяти
- ✅ **Производительность** - минимум вызовов, максимум инлайнинга
- ✅ **Embedded-friendly** - компактная память, нет vtable

## 🛡️ Безопасность

### Rule of 5 для Variant

```cpp
class Variant {
    // Копирование увеличивает refcount
    Variant(const Variant& other) {
        if (other.is_ptr()) script_ref(other.ptr_value);
        // ... копирование других полей
    }
    
    // Присваивание управляет refcount'ами
    Variant& operator=(const Variant& other) {
        if (this != &other) {
            if (is_ptr()) script_unref(ptr_value);  // Старый
            if (other.is_ptr()) script_ref(other.ptr_value); // Новый
            // ... копирование других полей
        }
        return *this;
    }
};
```

## 📊 Производительность

### Преимущества

- **Скрипты**: Прямой доступ к данным без оверхеда
- **VM**: Быстрые атомарные операции с refcount
- **Память**: Минимальный overhead (4 байта на объект)

### Ограничения

- Не поддерживает циклические ссылки
- Требует аккуратного использования в многопоточных средах

## 🔧 Использование в VM

### Типичный сценарий

```cpp
void execute_script() {
    Variant local_vars[10];
    
    // Создание объектов
    local_vars[0].set_vector3(1, 2, 3);  // ref_count = 1
    local_vars[1].set_string("test");    // ref_count = 1
    
    // Использование в нативном коде
    use_object(local_vars[0].get_ptr()); 
    
    // Автоматическое управление при выходе из scope
    // local_vars уничтожаются -> автоматически вызывается unref
}
```

``` cpp
// example_usage.cpp
# include "VirtualMachine.hpp"
# include "BinaryFile.hpp"

int main() {
    using namespace vm;

    // Инициализация VM
    VirtualMachine& vm = VirtualMachine::get_instance();
    
    // Загрузка бинарного файла с байткодом
    vm.load_binary_file("game_code.bin");
    
    // Создание процессов с self-указателями (наш базис!)
    void* player_obj = /* создание игрового объекта */;
    void* enemy_obj = /* создание вражеского объекта */;
    
    Process* player_process = vm.create_process("player", player_obj);
    Process* enemy_process = vm.create_process("enemy", enemy_obj);
    
    // Настройка FSM для врага
    auto enemy_fsm = std::make_unique<StateMachine>(enemy_process);
    
    // Добавление состояний с байткодом
    FunctionDesc* patrol_code = vm.find_function("patrol-behavior");
    FunctionDesc* chase_code = vm.find_function("chase-behavior");
    
    if (patrol_code && chase_code) {
        auto patrol_state = std::make_unique<StateDesc>("patrol", patrol_code);
        auto chase_state = std::make_unique<StateDesc>("chase", chase_code);
        
        enemy_fsm->add_state(std::move(patrol_state));
        enemy_fsm->add_state(std::move(chase_state));
        enemy_fsm->set_initial_state("patrol");
        
        enemy_process->set_state_machine(std::move(enemy_fsm));
    }
    
    // Главный игровой цикл
    for (int frame = 0; frame < 1000; ++frame) {
        vm.execute_frame(); // Выполняем один кадр всех процессов
        
        // Другие игровые системы...
    }
    
    return 0;
} // namespace vm
