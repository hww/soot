# GOAL: низкоуровневый справочник (обновлённая версия)

Документ описывает синтаксис и примитивы GOAL, важные для понимания того, **как компилятор обрабатывает код**: объявления, размещение в памяти, типы, методы, состояния, приведения. Игровая логика не рассматривается.

---

## 1. Базовые типы (built-in types)

Предоставляются ядром и runtime-ом.

### Числовые
- `int`, `uint` — регистровой ширины
- `int8/16/32/64/128`, `uint8/16/32/64/128`
- `float`
- `binteger` — упакованное целое (младшие 3 бита = 0)
- `sinteger`, `uinteger`

### Ссылочные
- `object` — базовый для всех ссылок
- `basic` — с runtime-заголовком (тип, size, method-table)
- `structure` — без заголовка
- `pointer` — нетипизированный указатель
- `function`
- `symbol`, `string`, `type`

### Специальные
- `none` — void
- `pair`
- `number`
- `array`, `inline-array`

```lisp
(define-extern foo int)
(define-extern bar (function int int int))
```

---

## 2. `basic` vs `structure`

Ключевое различие для компилятора: наличие runtime-заголовка.

| | `basic` | `structure` |
|---|---|---|
| Runtime-тип | да | нет |
| Методы | да | нет |
| `type-type?` | работает | нет |
| Размер | +заголовок | только данные |
| Создание | `object-new` | `object-new` |

```lisp
(deftype my-basic (basic)
  ((value int :offset 4)))

(deftype my-struct (structure)
  ((x float)
   (y float)))
```

Компилятор для `basic` генерирует таблицу методов и type-объект, для `structure` — только layout.

### `:no-runtime-type`

Если тип уже создан ядром (например, всё в `kernel-defs.gc`), компилятор **не должен** генерировать для него type-объект:

```lisp
(deftype process-tree (basic)
  ((name   basic :offset-assert 4)
   ...)
  :no-runtime-type)   ;; type уже есть в ядре
```

Без этого флага компилятор создаст второй type-объект, что вызовет конфликт при загрузке.

---

## 3. Декларация типов (`deftype`)

```lisp
(deftype process-tree (basic)
  ((name     basic :offset-assert 4)
   (parent   (pointer process-tree) :offset-assert 12)
   (child    (pointer process-tree) :offset-assert 20)
   (self     process-tree :offset-assert 28))
  (:methods
   (new (symbol type basic) _type_)
   (activate (_type_ process-tree basic pointer) process-tree))
  :size-assert #x20
  :method-count-assert 14
  :flag-assert #x90000000c)
```

Атрибуты полей, влияющие на layout:

- `:offset N`, `:offset-assert N` — фиксация смещения
- `:size N` — битовая ширина (bitfield)
- `:inline` — значение внутри структуры, не ссылка
- `:dynamic` — массив переменной длины в хвосте
- `:overlay-at path` — наложение поля на другое смещение (union)
- `:pack-me` — без выравнивания
- `:allow-misaligned` — разрешить невыровненный доступ

Атрибуты типа:

- `:size-assert` — проверка размера
- `:method-count-assert` — проверка числа методов
- `:flag-assert` — общая проверка layout
- `:no-runtime-type` — не генерировать type-объект (тип задан в ядре)
- `:state-methods` — список состояний (states)

### `:flag-assert`

Формат `#x90000000c`:

- старшие биты (`0x9...`) — сигнатура/магия типа
- младшие (`0xC` = 12) — размер заголовка

Проверяется при загрузке `.go`-файла; несоответствие — ошибка.

### Тип-примитив с overlay-полями

`deftype` может применяться **к числовому типу**, создавая битовую разметку:

```lisp
(deftype pat-surface (uint32)
  ((skip          uint8 :offset 0 :size 3)
   (mode          pat-mode :offset 3 :size 3)
   (material      pat-material :offset 6 :size 6)
   (camera        uint8 :offset 12 :size 2)
   (event         pat-event :offset 14 :size 6)
   (noentity      uint8 :offset 0 :size 1)
   (nocamera      uint8 :offset 1 :size 1)
   (noedge        uint8 :offset 2 :size 1)
   (nolineofsight uint8 :offset 12 :size 1)
   (unknown-bit   uint8 :offset 15 :size 1)))
```

Компилятор генерирует `srl`/`sll`/`and`/`or` для доступа к каждому полю. Размер типа фиксирован (uint32).

### `:overlay-at` через путь

`:overlay-at` принимает **путь**, разрешаемый в compile-time:

```lisp
(deftype surface (basic)
  ((name symbol)
   (turnv float)
   (data float 30 :overlay-at turnv)  ;; data[0] накладывается на turnv
   (hook function 4 :overlay-at active-hook)
   (dataw uint32 2 :overlay-at mode)))
```

`data[0]` использует то же смещение, что `turnv`. Выравнивание — по первому полю пути.

---

## 4. Enum и Bitfield

### Enum

```lisp
(defenum language-enum
  :type int64
  (english) (french) (german))
```

Значения присваиваются 0,1,2..., можно указать явно: `(english 5)`.

### Bitfield

```lisp
(defenum process-mask
  :bitfield #t
  :type uint32
  (execute 0)   ;; бит 0 → маска 1
  (draw 1)      ;; бит 1 → маска 2
  (pause 2))    ;; бит 2 → маска 4
```

Компилятор превращает имя в маску (сдвиг 1 на номер бита). Работа — через `logtest?`, `logior!`, `logclear!`, `logxor!`.

Bitfield может быть 64-битным:

```lisp
(defenum collide-kind
  :type uint64
  :bitfield #t
  (background 0)
  (cak-1 1)
  ...)
```

Для 64-битных bitfield компилятор генерирует `dsll`/`dsrl`/`dand`/`dor` вместо 32-битных.

---

## 5. Константы

```lisp
(defconstant COLLISION_MISS -100000000.0)
(defconstant QUAT_SCALE 0.000030517578125)
(defglobalconstant BIG_COLLIDE_CACHE_SIZE 5000)
```

- `defconstant` — попадает в kernel-defs, доступен на этапе компиляции
- `defglobalconstant` — тоже compile-time, но не экспортируется в kernel

`defglobalconstant` участвует в compile-time арифметике **до** генерации кода:

```lisp
(defglobalconstant BIG_COLLIDE_CACHE_SIZE 5000)
(#when PC_PORT
  (malloc 'global (* (size-of collide-cache-tri) BIG_COLLIDE_CACHE_SIZE)))
```

Глобальные переменные:

```lisp
(define *walk-mods* (new 'static 'surface ...))
(define-perm *camera* camera-master #f)  ;; сохраняется между загрузками уровня
```

### `define-perm` vs `define`

`define-perm` создаёт глобал, который **не сбрасывается** при загрузке/выгрузке уровня:

```lisp
(define-perm *collide-cache* collide-cache (new 'global 'collide-cache))
(define-perm *touching-list* touching-list (new 'global 'touching-list))
(define-perm *camera-init-mat* matrix #f)
(define-perm *external-cam-options* (external-cam-option))
```

`define` — обычная глобальная переменная, сбрасывается при загрузке уровня.

---

## 6. Forward declaration и extern

### `define-extern`
Объявление символа с типом без определения — для символов из C-ядра или других файлов.

```lisp
(define-extern #f symbol)
(define-extern function type)
(define-extern malloc (function symbol int pointer))
```

### `declare-type`
Предварительное объявление типа — разрывает циклы.

```lisp
(declare-type collide-shape basic)
(declare-type joint-control basic)
```

### `declare-file`
Помечает файл как debug-only; компилятор включает его только в debug-сборке.

```lisp
(declare-file (debug))
```

---

## 7. Методы: декларация и дефиниция

### Декларация в `deftype`

```lisp
(:methods
 (new (symbol type int) _type_)
 (current-cycle-distance (_type_) float))
```

Первый аргумент — `_type_` (`this`), последний — возвращаемый тип. Порядковый номер метода в `:methods` = индекс в method-table.

`_type_` — псевдо-тип, означающий «тип this». Может быть и возвращаемым типом:

```lisp
(current-cycle-distance (_type_) float)
```

### Дефиниция

```lisp
(defmethod new joint-control ((allocation symbol) (type-to-make type) (arg0 int))
  ...)
```

Синтаксис: `(defmethod имя Тип ((arg тип) ...) тело)`.

### Вызов родительского метода

```lisp
(defmethod params-init ((this barrel) (arg0 entity))
  (let ((t9-0 (method-of-type crate params-init))) (t9-0 this arg0))
  ...)
```

### `:replace`

Заменяет унаследованный метод, а не добавляет новый слот в method-table.

```lisp
(defmethod relocate ((this art-group) (arg0 kheap) (arg1 (pointer uint8)) none :replace)
  ...)
```

---

## 8. Создание экземпляров

### `object-new`

Низкоуровневое создание `basic`/`structure`:

```lisp
(object-new allocation type-to-make size)
```

Возвращает указатель или 0.

### `new`

Высокоуровневый конструктор с вызовом соответствующего `new`-метода:

```lisp
(new 'process 'collide-shape-moving this (collide-list-enum usually-hit-by-player))
```

Первый аргумент — allocation:
- `'static` — при загрузке, без runtime
- `'global` — глобальная куча
- `'process` — куча процесса
- `'debug` — debug-куча
- `'stack` — на стеке

### `new-dynamic-structure`

Для типов с `:dynamic`-массивом:

```lisp
(new-dynamic-structure allocation type-to-make
                       (the-as int (+ (-> type-to-make size) (* 48 n))))
```

### Удаление

```lisp
(delete-basic obj)
```

---

## 9. Функции

```lisp
(defun ray-sphere-intersect ((o vector) (d vector) (c vector) (r float))
  (local-vars (zero uint128) (v1-1 uint128))
  (the-as float result))
```

- `local-vars` — объявление переменных вне лексического блока (восстанавливает регистры).
- `(the-as T x)` — небезопасное приведение.
- `(the T x)` — безопасное (с runtime-проверкой).

### Анонимные

```lisp
(lambda ((arg0 surface) (arg1 object)) (set! (-> arg0 fric) 151756.8))
```

### Behavior

Тип-функция с поддержкой `suspend`, `go`, `return`:

```lisp
(defbehavior reset-follow camera-master ()
  (set! (-> self tpos-old quad) (-> (target-cam-pos) quad)))
```

Сигнатура: `(function ... :behavior Тип)`.

### `crash!` — примитив

```lisp
(defun raw-ray-sphere-intersect ((arg0 float))
  (crash!)
  ...)
```

Аналог `abort`/`break`. Используется там, где функция полагается на нестандартное соглашение вызова (например, аргументы в VF-регистрах).

### `defun-debug` / `defmethod-debug`

Функции, включаемые только в debug-сборке:

```lisp
(defun-debug joint-mod-debug-draw ((mod joint-mod)) ...)
```

### `asm-func` и `mips2c`

Функция, определённая на ассемблере — компилятор не генерирует для неё код:

```lisp
(def-mips2c cspace<-parented-transformq-joint! (function cspace transformq none))
(defmethod-mips2c "(method 26 collide-cache)" 26 collide-cache)
(def-mips2c calc-animation-from-spr (function (inline-array vector) int none))
```

- `def-mips2c` — для свободной функции: `(def-mips2c имя (function ...))`
- `defmethod-mips2c` — для метода: `(defmethod-mips2c "строка-сигнатуры" номер-метода тип)`

Строка в `defmethod-mips2c` — как компилятор видит сигнатуру (обычно `"(method N type)"`).

### `pc-port-do-*` — макросы-обёртки

```lisp
(defmacro pc-port-do-raw-ray-sphere-intersect (rad vf1-val vf2-val)
  "Calls to raw-ray-sphere-intersect should be replaced with this macro"
  ...)
```

Заменяют функции, полагающиеся на VF-регистры, на портируемые версии.

---

## 10. Массивы

### `inline-array`
Элементы лежат подряд; для классов — `inline-array-class` с обязательным `heap-base`.

```lisp
(deftype skeleton (inline-array-class)
  ((bones bone :inline :dynamic)))
(set! (-> skeleton heap-base) (the-as uint 96))

(deftype cspace-array (inline-array-class)
  ((data cspace :inline :dynamic)))
(set! (-> cspace-array heap-base) (the-as uint 32))
```

`heap-base` — смещение, с которого начинается первый элемент. Влияет на арифметику `(-> arr i)`.

### `array`
Массив ссылок; создаётся `boxed-array`:

```lisp
(new 'static 'boxed-array :type string "a" "b")
```

### Доступ

```lisp
(-> arr i)       ;; inline-массив / поле структуры
(at arr i)       ;; boxed-массив / pointer
(-> arr data i)
```

---

## 11. Dereference и приведение типов

- `(-> obj a b c)` — доступ к полю
- `(&-> obj a b)` — адрес поля
- `(&+ ptr n)` — арифметика указателей
- `(the-as T x)` — переинтерпретация битов
- `(the T x)` — проверка runtime-типа
- `(rtype-of x)` — получить runtime-тип
- `(type-type? t1 t2)` — проверка наследования

```lisp
(the-as float arg0)
(the collider arg0)
(type-type? (-> self type) process-drawable)
```

---

## 12. Inline, align, прочие атрибуты полей

- `:inline` — значение внутри структуры
- `:dynamic` — хвостовой массив переменной длины
- `:offset N` / `:offset-assert N` — смещение
- `:overlay-at` — union (несколько полей на одном смещении)
- `:size N` — битовая ширина
- `:pack-me` — плотная упаковка
- `:allow-misaligned` — разрешить невыровненный доступ

Пример union-а:

```lisp
(deftype surface (basic)
  ((name symbol)
   (data float 30 :overlay-at turnv)
   (hook function 4 :overlay-at active-hook)
   (dataw uint32 2 :overlay-at mode)))
```

---

## 13. Состояния (states)

```lisp
(defstate cam-fixed (camera-slave)
  :event   (behavior ((proc process) (argc int) (message symbol) (block event-message-block)) ...)
  :enter   (behavior () ...)
  :exit    (behavior () ...)
  :trans   (behavior () ...)
  :code    (behavior () ...)
  :post    (behavior () ...))
```

Слоты обрабатываются runtime-ом процесса. `:virtual #t` — состояние может быть переопределено в подтипе.

`state` — это **объект** с полями `event`, `enter`, `exit`, `trans`, `code`, `post`. Их можно доставать через `(-> state code)`:

```lisp
(defstate nav-enemy-patrol (babak)
  :virtual #t
  :code
    (behavior ()
      ...
      ((the-as (function none) (-> (method-of-type nav-enemy nav-enemy-patrol) code)))))
```

Переходы:

- `go` — переход по state-объекту
- `go-virtual` — переход по имени, разрешается через method-table (для `:virtual`)
- `cam-slave-go` — специализированная функция для camera-slave с доп. инициализацией

```lisp
(go-virtual wait)
(go (method-of-object this nav-enemy-idle))
(cam-slave-go cam-free-floating)
```

---

## 14. `process-spawn` и `process-spawn-function`

Создание процесса с автоматической инициализацией:

```lisp
(process-spawn money :init money-init-by-other-no-bob
                s4-1 *null-vector* 5 1.0 (-> self entity) :to self)
```

- `:init` принимает **имя behavior-функции**, аргументы после него передаются в behavior
- `:from` — пул процессов
- `:to` — родитель
- `:name`, `:stack-size`, `:stack`

Функциональная форма — первым аргументом имя behavior, вторым — имя init-функции (обычной, не behavior):

```lisp
(process-spawn-function list-keeper list-keeper-init
                        :from *camera-dead-pool* :to self)
```

---

## 15. Метки и переходы

Восстановлены из MIPS-ветвлений:

```lisp
(b! #t cfg-13)
(label cfg-13)

(b! (< x y) cfg-5)
(b! (zero? a2-4) cfg-6 :delay (nop!))
```

`b!` — безусловный/условный переход; `:delay` — delay slot.

---

## 16. Inline-assembly и VU

```lisp
(rlet ((acc :class vf)
       (vf0 :class vf)
       (vf1 :class vf))
  (init-vf0-vector)
  (.lvf vf1 (&-> arg1 quad))
  (.mul.x.vf vf2 vf2 vf3))
```

- `rlet` — резервирование VU-регистров
- `with-vf`, `with-vf0`, `with-sp` — временные окружения
- `(break!)` — аналог int3
- `.lvf`, `.svf`, `.mul.x.vf` и т.п. — VU-инструкции

### `with-dma-buffer-add-bucket` и `with-cnt-vif-block`

Макросы для работы с DMA:

```lisp
(with-dma-buffer-add-bucket ((v1-7 (-> (current-frame) debug-buf)) bucket)
  (with-cnt-vif-block (v1-7)
    (dma-buffer-add-gif-tag v1-7 ...)
    (dma-buffer-add-uint128 v1-7 ...)))
```

---

## 17. Регистровые переменные

```lisp
(local-vars (zero uint128) (v1-1 uint128) (f31-0 none))
```

Объявляет переменные, которые компилятор размещает в регистрах; часто восстанавливаются декомпилятором. Декомпилятор не всегда их сохраняет — иногда `t9-1`, `s5-0` и т.п. появляются без объявления.

---

## 18. Директивы препроцессора

```lisp
(#when PC_PORT ...)
(#unless PC_PORT ...)
(#if PC_PORT ... ...)
```

Применяются на этапе компиляции для платформо-зависимого кода.

**Важно:** `#if` — это **терм**, возвращающий значение, а не директива:

```lisp
(defconstant DEBUG_SPHERE_SEC_AMT (#if (user? dass) 5 10))
```

`(user? имя)` — compile-time проверка имени пользователя.

---

## 19. Пакеты и зависимости

```lisp
(in-package goal)
(bundles "ENGINE.CGO" "GAME.CGO")
(require "engine/math/vector.gc")
```

`require` — compile-time гарантия, что файл уже загружен.

---

## 20. Макросы и quoting

```lisp
(defmacro ja (&key (chan 0) &key (group! #f) ...)
  `(let ((ja-ch (-> self skel root-channel ,chan)))
     ,(if frame-interp `(set! (-> ja-ch frame-interp) ,frame-interp) `(none))
     ...))
```

- `` ` `` — quasiquote
- `,` — unquote
- `,@` — splice
- `'sym`, `'(a b)` — quote

### `&key` и `&rest`

```lisp
(defmacro ja (&key (chan 0) &key (group! #f) ...) ...)
(defmacro manipy-spawn (trans entity skel arg &key (from *default-dead-pool*) ...) ...)
```

### `apply-i` — генерация begin-блока

```lisp
(defmacro defskelgroup (name art-name joint-geom joint-anim lods &key ...)
  `(... ,@(apply-i (lambda (x i) `(...)) lods) ...))
```

### Compile-time таблицы в макросах

Макросы могут обращаться к **compile-time таблицам**:

```lisp
(defmacro joint-node-index (jg-name name)
  (let ((jg-info (hash-table-try-ref *jg-info* (symbol->string jg-name))))
    ...))
```

Позволяет `(joint-node-index babak-lod0-jg "align")` разворачиваться в числовой литерал.

---

## 21. Регистрация ассетов в compile-time

```lisp
(def-art-elt babak-ag babak-lod0-jg 0)
(def-joint-node babak-lod0-jg "align" 1)
(def-tex bigpuff effects 0)
```

Заполняют таблицы:

- `def-art-elt` → `*art-info*` (`ag-name → {elt-name → index}`)
- `def-joint-node` → `*jg-info*` (`jg-name → {joint-name → node-index}`)
- `def-tex` → `*tex-info*` (`tex-name → {page → index}`)

Используются макросами `defskelgroup`, `joint-node`, `art-elt-index`.

```lisp
(defskelgroup *babak-sg*
  babak babak-lod0-jg -1
  ((babak-lod0-mg (meters 20)))
  :bounds (static-spherem 0 2 0 3))
```

---

## 22. `add-connection` и `define-perm`

```lisp
(add-connection *debug-engine* self camera-master-debug self #f #f)
(define-perm *camera* camera-master #f)
```

`define-perm` — глобальная переменная, сохраняющаяся между загрузками уровня.

---

## 23. Метки в сгенерированном коде

Декомпилятор генерирует блоки с метками:

```lisp
(b! #t cfg-13 :delay (nop!))
(set! v1-1 (the-as collide-work 0))
(label cfg-3)
(.add.y.vf.x vf8 vf8 vf8)
(b! (nonzero? a0-4) cfg-3 :delay (nop!))
(label cfg-6)
```

Это прямое отражение MIPS-ветвлений, важно для чтения декомпилированного кода.

---

## 24. `with-pp`

Доступ к неявному указателю процесса (`pp`), хранящемуся в регистре:

```lisp
(defun cam-slave-go ((arg0 state))
  (with-pp
    (cam-slave-init-vars)
    (let ((t9-1 (the-as (function object) enter-state)))
      (set! (-> pp next-state) arg0)
      (t9-1))))
```

---

## 25. `format` и `*temp-string*`

```lisp
(format (clear *temp-string*) "~S~S" arg0 arg1)
```

`(clear *temp-string*)` возвращает переиспользуемый буфер (обнуляет длину).

### Спецификаторы `format`

- `~S` — symbol/string с quote
- `~A` — aesthetic (без quote)
- `~D` — decimal
- `~F` — float
- `~X` — hex
- `~P` — print структуры
- `~M` — meters (делит на METER_LENGTH)
- `~R` — radians (делит на 182.04445)
- `~G` — degrees (умножает)
- `~%` — newline

`~M`, `~R`, `~G` конвертируют игровые единицы в человекочитаемые.

---

## 26. Инкрементальные и битовые операции

```lisp
(+! a b)   (*! a b)   (-! a b)   (/! a b)
(&+! ptr n)
(logior! x m)   (logand! x m)   (logxor! x m)   (logclear! x m)
```

Компилятор разворачивает их в соответствующие MIPS-инструкции с записью результата обратно в переменную.

---

## 27. `defenum` vs `defconstant`

- `defenum` — генерирует набор констант с автоинкрементом
- `defconstant` — одна константа
- `defglobalconstant` — константа без попадания в kernel-defs, участвует в compile-time арифметике

---

## 28. Единицы измерения (compile-time макросы)

```lisp
(meters x)    ;; x * 4096.0 (METER_LENGTH)
(seconds x)   ;; x * 60.0
(degrees x)   ;; x * 182.04445
```

Это **макросы**, а не функции. Раскрываются в числовой литерал на этапе компиляции.

```lisp
:transv-max 40960.0
:target-speed 40960.0
:wall-angle 0.2
:gravity -22.755556
```

Эквивалентны вызовам `(meters 10)`, `(seconds 0.2)`, `(degrees 2880)` и т.п.

---

## 29. Static-объекты

```lisp
(static-spherem 0 2 0 3)         ;; sphere в метрах
(static-bspherem 0 0 0 1)        ;; bsphere в метрах
(static-sound-spec "yel-eco-idle" :fo-max 15)
(new 'static 'boxed-array :type string "a" "b")
(new 'static 'rgba :r #xff :a #x80)
```

`static-*` — макросы, создающие static-объекты с указанием полей. `'static` allocation означает, что объект создаётся при загрузке `.go`-файла и не требует runtime-инициализации.

---

## 30. Системы частиц

```lisp
(defpartgroup group-eco-blue
  :id 42
  :bounds (static-bspherem 0 0 0 1)
  :parts
  ((sp-item 2 :flags (launch-asap) :binding 3)
   (sp-item 3 :fade-after (meters 40) :flags (start-dead launch-asap) :binding 5)
   ...))

(defpart 2
  :init-specs
  ((:texture (hotdot effects))
   (:num 1.0)
   (:x (meters 4))
   (:scale-x (meters 0.01))
   (:timer (seconds -0.005))
   (:flags (bit3 bit14))
   (:func 'sparticle-track-root-prim)))
```

- `defpartgroup` с `:id N` регистрируется в `*part-group-id-table*` по номеру
- `defpart` определяет параметры конкретной частицы
- Доступ: `(-> *part-group-id-table* 56)`, `(-> *part-id-table* 2528)`

```lisp
(set! (-> this eco-effect) (-> *part-group-id-table* 56))
(set! (-> *part-id-table* 118 init-specs 4 initial-valuef) (+ 24576.0 f30-1))
```

---

## Что важно понимать про компилятор

1. **Layout фиксируется явно** через `:offset`, `:size`, `:flag-assert`, `:size-assert`. Ошибки layout ловятся на этапе загрузки.
2. **`basic`/`structure`** — фундаментальное разделение: `basic` даёт runtime-тип, `structure` — нет.
3. **`deftype` может применяться к числовым типам** (`uint32`, `uint64`) для создания битовой разметки.
4. **Методы** нумеруются порядком в `:methods`; `:replace` и `method-of-type` управляют таблицей.
5. **States** — это объекты с полями `event/enter/exit/trans/code/post`; runtime вызывает их по слотам.
6. **`b!`/`label`** — прямое отражение MIPS-ветвлений.
7. **`the-as` vs `the`** — первое без проверки, второе с runtime-проверкой.
8. **`local-vars`** — восстановление регистровых переменных.
9. **`#when`, `#unless`, `#if`** — препроцессор; `#if` — терм, возвращающий значение.
10. **`def-art-elt`, `def-joint-node`, `def-tex`** — заполняют compile-time таблицы для `defskelgroup` и макросов доступа к узлам.
11. **`defglobalconstant`** участвует в compile-time арифметике (в т.ч. в `malloc`).
12. **`define-perm`** сохраняется между загрузками уровня; `define` — нет.
13. **`meters`/`seconds`/`degrees`** — compile-time макросы, а не функции.
14. **`def-mips2c` vs `defmethod-mips2c`** — первый для функций, второй для методов (с сигнатурой-строкой).
15. **`process-spawn`** принимает behavior в `:init`; **`process-spawn-function`** принимает behavior и init-функцию раздельно.
16. **`go` vs `go-virtual`** — первый по объекту, второй по имени через method-table.

---

## Итог

Документ покрывает все основные формы GOAL, встречающиеся в `combined_code.txt`:

- **Типы и layout:** `deftype`, `:offset`, `:size`, `:inline`, `:dynamic`, `:overlay-at`, `:no-runtime-type`, `:flag-assert`
- **Макросы:** `defmacro`, `&key`, `&rest`, `apply-i`, quasiquote
- **Ассеты:** `def-art-elt`, `def-joint-node`, `def-tex`, `defskelgroup`, `defpart`, `defpartgroup`
- **Методы и states:** `defmethod`, `method-of-type`, `:replace`, `defstate`, `:virtual`, `go`, `go-virtual`
- **Примитивы:** `object-new`, `new`, `new-dynamic-structure`, `process-spawn`, `process-spawn-function`
- **Compile-time:** `defglobalconstant`, `#if`, `#when`, `#unless`, `user?`, `hash-table-try-ref`
- **Единицы:** `meters`, `seconds`, `degrees`
- **MIPS2C:** `def-mips2c`, `defmethod-mips2c`, `pc-port-do-*`
- **DMA/VU:** `with-dma-buffer-add-bucket`, `with-cnt-vif-block`, `rlet`, `crash!`