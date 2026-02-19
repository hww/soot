# ZASM — Краткая документация

## Содержание
1. [Базовый синтаксис](#базовый-синтаксис)
2. [Инструкции Z80](#инструкции-z80)
3. [Метки и адресация](#метки-и-адресация)
4. [Директивы данных](#директивы-данных)
5. [Управление адресами](#управление-адресами)
6. [Модули и сегменты](#модули-и-сегменты)
7. [Регистровые псевдонимы (rlet)](#регистровые-псевдонимы-rlet)
8. [Процедуры (defproc)](#процедуры-defproc)
9. [Методы и ООП (defmethod)](#методы-и-ооп-defmethod)
10. [Виртуальные таблицы](#виртуальные-таблицы-vtable)
11. [Вызов методов (dcall/ycall)](#вызов-методов-dcallycall)
12. [Структуры и данные](#структуры-и-данные-static-new)
13. [Директивы высокого уровня](#директивы-высокого-уровня)
14. [Сохранение результатов](#сохранение-результатов)

---

## Базовый синтаксис

Ассемблерный код пишется внутри макроса `zasm`:

```lisp
(zasm
  (.ld a #x41)
  (.call 'print_char)
  (.ret))
```

Все инструкции начинаются с точки: `.ld`, `.add`, `.jr` и т.д.

---

## Инструкции Z80

### Пересылка данных (LD)

```lisp
;; 8-битные регистры
(.ld a b)           ; LD A, B
(.ld c #x41)        ; LD C, $41
(.ld d (@ hl))      ; LD D, (HL) — чтение из памяти

;; 16-битные регистры
(.ld hl #x8000)     ; LD HL, $8000
(.ld bc #x1234)     ; LD BC, $1234

;; Индексные регистры IX/IY
(.ld ix #x9000)     ; LD IX, $9000
(.ld iy #xA000)     ; LD IY, $A000

;; Косвенная адресация
(.ld a (@ bc))      ; LD A, (BC)
(.ld (@ de) a)      ; LD (DE), A

;; Индексная адресация с IX/IY
(.ld a (+ix 5))     ; LD A, (IX+5)
(.ld (+iy 3) b)     ; LD (IY+3), B
(.ld (+ix 2) #xFF)  ; LD (IX+2), $FF
```

### Арифметика и логика

```lisp
;; 8-битные операции с аккумулятором
(.add a b)          ; ADD A, B
(.add a #x10)       ; ADD A, $10
(.sub c)            ; SUB C
(.sub #x20)         ; SUB $20
(.and d)            ; AND D
(.or e)             ; OR E
(.xor #xFF)         ; XOR $FF
(.cp h)             ; CP H
(.cp #x41)          ; CP $41

;; 16-битная арифметика
(.add hl bc)        ; ADD HL, BC
(.add hl de)        ; ADD HL, DE
(.adc hl bc)        ; ADC HL, BC
(.sbc hl de)        ; SBC HL, DE
```

### Инкремент/декремент

```lisp
(.inc a)            ; INC A
(.dec b)            ; DEC B
(.inc hl)           ; INC HL
(.dec ix)           ; DEC IX
```

### Сдвиги и вращения

```lisp
;; Через префикс CB
(.rlc b)            ; RLC B
(.rrc c)            ; RRC C
(.rl d)             ; RL D
(.rr e)             ; RR E
(.sla h)            ; SLA H
(.sra l)            ; SRA L
(.srl a)            ; SRL A

;; Быстрые сдвиги аккумулятора
(.rla)              ; RLA
(.rra)              ; RRA
```

### Битовые операции

```lisp
(.bit 3 a)          ; BIT 3, A
(.set 5 b)          ; SET 5, B
(.res 0 c)          ; RES 0, C
```

### Переходы и вызовы

```lisp
;; Безусловные переходы
(.jp #x8000)        ; JP $8000
(.jp hl)            ; JP (HL)
(.jp ix)            ; JP (IX)
(.jr #x100)         ; JR $100 (относительный)

;; Условные переходы
(.jp 'z #x8000)     ; JP Z, $8000
(.jp 'nc #x8100)    ; JP NC, $8100
(.jr 'nz #x50)      ; JR NZ, $50

;; Вызовы и возвраты
(.call #x8000)      ; CALL $8000
(.call 'printf)     ; CALL printf
(.ret)              ; RET
(.ret 'z)           ; RET Z
(.ret 'c)           ; RET C

;; Специальные
(.rst #x10)         ; RST $10
(.djnz 'loop)       ; DJNZ loop
```

### Стек

```lisp
(.push hl)          ; PUSH HL
(.push ix)          ; PUSH IX
(.pop de)           ; POP DE
(.pop iy)           ; POP IY
(.ex de hl)         ; EX DE, HL
(.ex af af)         ; EX AF, AF'
```

### Ввод-вывод и прерывания

```lisp
(.in a (#xFE))      ; IN A, ($FE)
(.out (#xFE) a)     ; OUT ($FE), A
(.di)               ; DI
(.ei)               ; EI
(.im 1)             ; IM 1
(.halt)             ; HALT
(.reti)             ; RETI
(.retn)             ; RETN
```

### Блочные операции

```lisp
(.ldir)             ; LDIR
(.lddr)             ; LDDR
(.cpir)             ; CPIR
```

### Прочие

```lisp
(.nop)              ; NOP
(.neg)              ; NEG
(.daa)              ; DAA
(.cpl)              ; CPL
(.scf)              ; SCF
(.ccf)              ; CCF
```

---

## Метки и адресация

### Определение меток

```lisp
(zasm
  (.label start)            ; простая метка
  (.label main :scope 'public)  ; публичная метка (видна из других модулей)
  (.label data :address #x8000) ; метка по конкретному адресу
  
  (.ld a #x41)
  (.jr 'start)              ; переход на метку
  (.jp 'main))
```

### Получение адреса метки

```lisp
;; Внутри выражений Lisp
(address-of 'start)         ; → число (адрес)

;; В ассемблерном коде
(.ld hl 'start)             ; LD HL, start (адрес метки)
(.dw 'start)                ; DW start (16-битный адрес)
```

### Косвенная и индексная адресация

```lisp
;; Косвенная (через регистр)
(.ld a (@ hl))              ; LD A, (HL)
(.ld (@ de) b)              ; LD (DE), B

;; Индексная с IX
(.ld a (+ix 5))             ; LD A, (IX+5)
(.ld (+iy 3) #xFF)          ; LD (IY+3), $FF

;; Через макросы для структур
(.ld a (->ix vec3 y))       ; LD A, (IX+2) — автоматическое смещение
```

---

## Директивы данных

```lisp
;; Байты
(.db #x41 #x42 #x43)        ; три байта: 41 42 43
(.db "Hello")               ; строка как байты (без нуля!)
(.db "World" 0)             ; строка с нуль-терминатором

;; Слова (16 бит, little-endian)
(.dw #x1234)                ; 34 12
(.dw 'start)                ; адрес метки start

;; Блоки памяти
(.ds 16)                    ; 16 байт, заполненных 0
(.ds 32 #xFF)               ; 32 байта, заполненных $FF
```

---

## Управление адресами

```lisp
(.org #x8000)               ; установить текущий адрес в $8000

(.align 16)                 ; выровнять до границы 16 байт
(.align 256 #x00)           ; выровнять до 256, заполняя $00

;; Организация кода
(zasm
  (.org #x0000)
  (.label reset_vector)
  (.jp 'start)
  
  (.org #x0038)
  (.label interrupt_vector)
  (.jp 'isr)
  
  (.org #x8000)
  (.label start)
  (.ld sp #xFFFF)
  (.ei))
```

---

## Модули и сегменты

### Определение модуля

```lisp
;; В начале файла
(defmodule myprogram)

;; Переключение контекста
(with-module myprogram
  (with-segment 'code
    (zasm ...)))
```

Обычно модуль задаётся один раз в начале, затем все `zasm` автоматически попадают в него.

---

## Регистровые псевдонимы (rlet)

`rlet` позволяет связать логические переменные с физическими регистрами:

```lisp
(defproc process-vector ()
  (rlet ((x int16 hl)        ; переменная x в регистре HL
         (y int16 de)         ; y в DE
         (z int16 bc)         ; z в BC
         (tmp int8 a))        ; tmp в A
    
    (zasm
      (.add hl de)            ; x + y
      (.add hl bc)            ; + z
      (.ld a l)               ; младший байт результата в tmp
      (.ret))))
```

Это документирует использование регистров и позволяет проверять типы.

---

## Процедуры (defproc)

```lisp
;; Простая процедура без аргументов
(defproc delay ()
  (declare (once) (asm-func none))
  (zasm
    (.label loop)
    (.dec bc)
    (.ld a b)
    (.or c)
    (.jr 'nz 'loop)
    (.ret)))

;; Процедура с регистровыми аргументами
(defproc copy-string ((dest string :reg de) 
                       (src string :reg hl))
  (declare (once) (asm-func none))
  (zasm
    (.label loop)
    (.ld a (@ hl))
    (.ld (@ de) a)
    (.or a)
    (.ret 'z)
    (.inc hl)
    (.inc de)
    (.jr 'loop)))

;; Процедура, возвращающая значение
(defproc sum ((a int16 hl) (b int16 de) (c int16 bc))
  (declare (once) (asm-func int16))  ; возвращает int16
  (rlet ((result int16 hl))
    (zasm
      (.add hl de)
      (.add hl bc)
      (.ret))))
```

---

## Методы и ООП (defmethod)

### Определение типа

```lisp
(deftype vec3 (structure)
  ((x int16) 
   (y int16) 
   (z int16))
  (:methods
    (new (int16 int16 int16) _type_)
    (len () int16)
    (clear () none)))
```

### Метод экземпляра (instance method)

```lisp
(defmethod len ((self vec3 ix))  ; self в регистре IX
  (declare (once) (asm-func int16))
  (rlet ((result int16 hl))
    (zasm
      (.ld hl (-> self x))       ; HL = self.x
      (.ld de (-> self y))       ; DE = self.y
      (.add hl de)               ; HL = x + y
      (.ld de (-> self z))       ; DE = self.z
      (.add hl de)               ; HL = x + y + z
      (.ret))))
```

### Метод-конструктор (static method)

```lisp
(defmethod new vec3 ((x int16 hl) (y int16 de) (z int16 bc))
  (declare (once) (asm-func _type_))
  (rlet ((this pointer ix))
    (zasm
      ;; Адрес нового объекта предположительно в IX
      (.ld hl 'vec3::vtable)
      (.ld (+ix 0) l)            ; сохраняем vtable
      (.ld (+ix 1) h)
      (.ld (-> this x) hl)       ; this.x = x
      (.ld (-> this y) de)       ; this.y = y
      (.ld (-> this z) bc)       ; this.z = z
      (.ret))))
```

### Метод с побочными эффектами

```lisp
(defmethod clear ((self vec3 ix))
  (declare (once) (asm-func none))
  (zasm
    (.ld (-> self x) 0)
    (.ld (-> self y) 0)
    (.ld (-> self z) 0)
    (.ret)))
```

---

## Виртуальные таблицы (vtable)

### Генерация VTable

```lisp
(defproc setup-vtables ()
  (declare (once) (asm-func none))
  (zasm
    ;; Генерируем VTable для типа vec3
    (.vtable vec3)))
```

Это создаст таблицу вида:
```lisp
vec3::vtable:
  JP vec3::new      ; слот 0
  JP vec3::len      ; слот 1
  JP vec3::clear    ; слот 2
```

### Прокси-таблица (для драйверов/плагинов)

```lisp
(defproc setup-proxy ()
  (declare (once) (asm-func none))
  (zasm
    ;; Создаём прокси-таблицу в RAM
    (.vtable-proxy vec3 'vec3-active)))
    
;; Переключение драйвера
(defproc switch-to-vec3 ()
  (declare (once) (asm-func none))
  (zasm
    (.switch-vtable vec3 'vec3-active)))
```

---

## Вызов методов (dcall/ycall)

### Прямой вызов (dcall)

```lisp
(defproc test-direct-call ()
  (declare (once) (asm-func none))
  (zasm
    ;; Вызов статического метода
    (dcall vec3::new int16 int16 int16 _type_)
    
    ;; Вызов метода экземпляра
    (.ld ix #x8000)              ; объект в IX
    (dcall vec3::len int16)))
    
    ;; Вызов процедуры
    (dcall delay none)))
```

### Вызов через VTable (ycall)

```lisp
(defproc test-virtual-call ()
  (declare (once) (asm-func none))
  (rlet ((len int16 hl))
    (zasm
      ;; Объект в IX
      (.ld ix #x8000)
      
      ;; Загружаем адрес метода из VTable
      (obj-method->iy ix vec3::len)
      
      ;; Вызываем через IY
      (ycall vec3::len vec3 len))))
```

### Получение адреса метода

```lisp
(obj-method->iy ix vec3::len)  ; IY = адрес метода len для объекта в IX
```

---

## Структуры и данные (.static-new)

```lisp
;; Определение структуры
(deftype point (structure)
  ((x int16)
   (y int16)))

;; Статическое создание экземпляра
(defproc create-data ()
  (declare (once) (asm-func none))
  (zasm
    (.static-new p1 point
      :x 100
      :y 200)))
```

Это создаст в коде:
```lisp
p1:
p1::x:  DW 100
p1::y:  DW 200
```

---

## Директивы высокого уровня

### Комментарии и отступы

```lisp
(zasm
  (emit-space)                    ; пустая строка в листинге
  (emit-space "Инициализация")    ; разделитель с заголовком
  (emit-comment "Это комментарий")
  
  (.ld a #x41)                     ; сам код
  (emit-comment "вывод символа")
  (.call 'putchar))
```

### Выражения

```lisp
(define BASE #x4000)

(zasm
  ;; Адрес вычисляется во время компиляции
  (.ld hl (+ BASE #x100))          ; HL = $4100
  
  ;; Сложные выражения
  (.ld bc (+ (address-of 'start) 10)))
```

---

## Сохранение результатов

### Конфигурация проекта

```lisp
;; Определяем, какие сегменты в какие файлы сохранять
(define *project-layout* 
  '(("firmware.hex" "code")        ; сегмент code → firmware.hex
    ("vectors.hex"  "interrupts"))) ; сегмент interrupts → vectors.hex
```

### Сохранение

```lisp
;; После компиляции
(save-module-practical 'default *project-layout*)

;; Символы для отладчика
(export-symbols-to-emu "firmware.sym" 'default)

;; Паспорт модуля (для линковки)
(save-module-passport "firmware.passport" 'default)
```

### Компиляция модуля

```lisp
;; Двухпроходная компиляция
(assemble-module 'default)

;; Просмотр листинга
(dump-listing 'default)
```

---

## Пример полной программы

```lisp
;; Загружаем ассемблер
(load "common/xiff/soot/zasm/zasm.sot")

;; Определяем типы
(deftype vec3 (structure)
  ((x int16) (y int16) (z int16))
  (:methods
    (new (int16 int16 int16) _type_)
    (len () int16)))

;; Реализация метода
(defmethod len ((self vec3 ix))
  (declare (once) (asm-func int16))
  (zasm
    (.ld hl (-> self x))
    (.ld de (-> self y))
    (.add hl de)
    (.ld de (-> self z))
    (.add hl de)
    (.ret)))

;; Процедура инициализации
(defproc init ()
  (declare (once) (asm-func none))
  (zasm
    (.org #x8000)
    (.label start)
    (.ld sp #xFFFF)
    
    ;; Создаём объект
    (.ld hl 10)        ; x = 10
    (.ld de 20)        ; y = 20
    (.ld bc 30)        ; z = 30
    (dcall vec3::new int16 int16 int16 _type_)
    
    ;; Вычисляем длину
    (dcall vec3::len int16)
    
    ;; Бесконечный цикл
    (.label loop)
    (.jr 'loop)))

;; Компилируем
(assemble-module 'default)

;; Сохраняем
(save-module-practical 'default '(("program.hex" "code")))
```