# Система команд VM dconstruct — детальное описание

Ниже — развёрнутое описание каждой мнемоники с реальной семантикой, взятой из интерпретатора (`parse_instruction.cpp`), дизассемблера (`disassembler.cpp`) и кодогенератора (`ast/**`). Формат инструкции: **4 байта — `opcode, dest, op1, op2`**. Регистры `r0..r48` — общего назначения, `r49+` — аргументы (`arg_0 = r49`). `r0` — регистр возврата.

---

## 0. Общая модель исполнения

- **Стек-фрейм** (`ScriptStackFrame`) содержит массив из 50+ регистров, symbol table и указатель на инструкции.
- **Регистры** — 64-битные ячейки. Тип значения отслеживается отдельно (`Register::m_type`), потому что одна и та же ячейка может хранить `i64`, `f32`, `ptr` и т.д.
- **Symbol table** — массив 64-битных значений, индексируется `op1`. Может содержать: целые, f32, указатели, строки, SID, указатели на функции.
- **Истина/ложь**: `true` = `qword_1484B6450` (обычно 1), `false` = `qword_1484B6460` (обычно 0). Эти значения кэшируются в `v17`/`v18` интерпретатора.
- **Указатели**: хранятся как абсолютные адреса в момент исполнения; при загрузке файла reloc-таблица прибавляет базу (`m_bytes.get()`).

---

## 1. Арифметика с целыми

### `IAdd` — целочисленное сложение

```
dest = op1 + op2
```

**Особенность:** если `op1` — указатель (`is_pointer()`), то `dest` наследует тип и значение указателя, а `m_pointerOffset` увеличивается на `m_pointerOffset` операнда 2. В кодогенераторе (`add.cpp`) при `ptr_size > 1` дополнительно вставляется `IMulImm` для масштабирования индекса.

### `ISub` — целочисленное вычитание

```
dest = op1 - op2
```

Устанавливает CF (carry flag) при `op1 < op2` — используется в `FLessThan`/`FGreaterThan` для проверки знака.

### `IMul` — целочисленное умножение

```
dest = op1 * op2
```

Устанавливает OF при переполнении 128-битного произведения.

### `IDiv` — целочисленное деление

```
dest = op1 / (i64)op2
```

Деление знаковое (`i64`). Деление на 0 не проверяется — падает.

### `IMod` — целочисленный остаток

```
dest = (i64)op1 % op2
```

### `INeg` — унарное отрицание

```
dest = -op1
```

Устанавливает CF, если `op1 != 0`.

### `IAbs` — модуль целого

```
dest = abs(op1)
```

Реализовано через трюк: `v31 = (i64)op1; *(u64*)&v31 = *((u64*)&v31 + 1) ^ v31; dest = v31 - *((u64*)&v31 + 1)` — классический branchless abs.

### `IAddImm` — сложение с imm16

```
dest = op1 + (u16)op2
```

**Особенность:** если `op1` — указатель, `dest` наследует указатель и увеличивает `m_pointerOffset` на `op2`. В декомпиляторе (`disassembler.cpp`) это используется для отображения `rX = ptr + N -> <resolved>`.

### `ISubImm` / `IMulImm` / `IDivImm` — аналогично с imm16

```
dest = op1 - op2
dest = op1 * op2
dest = op1 / op2
```

`IMulImm` часто генерируется для индексации массивов (умножение индекса на `sizeof(element)`).

### `IntAsh` — арифметический сдвиг

```
if (op2 < 0) dest = op1 >> -op2
else         dest = op1 << op2
```

Знаковый сдвиг: если `op2` отрицательный — вправо, иначе — влево.

---

## 2. Арифметика с плавающей точкой

### `FAdd` / `FSub` / `FMul` / `FDiv`

```
dest = op1 OP op2   (f32)
```

Все операции — 32-битные float.

### `FMod` — остаток f32

```
dest = fmodf(op1, op2)
```

Вызывается libm-функция `fmodf`.

### `FNeg` — отрицание f32

```
dest = -op1
```

Через XOR с `xmm7` (маска знака).

### `FAbs` — модуль f32

```
dest = abs(op1)
```

Через AND с `xmm6` (маска 0x7FFFFFFF).

---

## 3. Сравнения

Все сравнения возвращают **булево значение** в `dest`, используя кэшированные `v17` (true) и `v18` (false).

### `IEqual`

```
dest = (op1 == op2) ? true : false
```

### `INotEqual`

```
dest = (op1 != op2) ? true : false
```

### `IGreaterThan`

```
dest = ((i64)op1 > (i64)op2) ? true : false
```

### `IGreaterThanEqual`

```
dest = ((i64)op1 >= (i64)op2) ? true : false
```

### `ILessThan`

```
dest = ((i64)op1 < (i64)op2) ? true : false
```

### `ILessThanEqual`

```
dest = ((i64)op1 <= (i64)op2) ? true : false
```

### `FEqual` / `FNotEqual`

```
dest = (op1 == op2) / (op1 != op2)
```

Используют `vucomiss` (unordered compare). `FNotEqual` истинно, если операнды не равны **или** один из них NaN.

### `FGreaterThan` / `FGreaterThanEqual` / `FLessThan` / `FLessThanEqual`

```
dest = (op1 OP op2) ? true : false
```

Проверка через `vcomiss` + carry flag. NaN обрабатывается отдельно.

---

## 4. Логические и битовые

### `OpLogAnd` — логическое И

```
dest = (op1 && op2) ? true : false
```

**Short-circuit не выполняется** на уровне VM — оба операнда уже вычислены. Short-circuit реализуется на уровне кодогенератора через `BranchIfNot` (см. `logical.cpp`).

### `OpLogOr` — логическое ИЛИ

```
dest = (op1 || op2) ? true : false
```

### `OpLogNot` — логическое НЕ

```
dest = (op1 == 0) ? true : false
```

### `OpBitAnd` / `OpBitOr` / `OpBitXor`

```
dest = op1 & op2
dest = op1 | op2
dest = op1 ^ op2
```

### `OpBitNot`

```
dest = ~op1
```

### `OpBitNor`

```
dest = ~(op1 | op2)
```

В декомпиляторе (`bitwise_and.cpp`) есть баг: `bitwise_and_expr::emit_dc` генерирует `OpBitOr` вместо `OpBitAnd` — это, вероятно, ошибка копипасты, но она существует в коде.

---

## 5. Пересылки

### `Move` — универсальная пересылка

```
dest = op1
```

Копирует **всё**: значение, тип, `m_pointerOffset`, `m_containsArg`. В интерпретаторе также копирует вспомогательный массив `v13` (типы/метаданные).

### `MoveInt`

```
dest = op1   (тип i64)
```

Используется для явного копирования целых.

### `MoveFloat`

```
dest = op1   (тип f32)
```

Копирует как f32 (одна ячейка).

### `MovePointer`

```
dest = op1   (тип ptr)
```

Копирует указатель и его offset.

---

## 6. Приведение типов

### `CastInteger`

```
dest = (u64)(f32)dest
```

**In-place**: интерпретирует `dest` как f32, преобразует в целое, кладёт обратно. Используется для `(i32)float`.

### `CastFloat`

```
dest = (f32)(u64)dest
```

**In-place**: целое → f32.

---

## 7. Загрузка констант

### `LoadU16Imm` — 16-битная константа

```
dest = op1 | (op2 << 8)
```

Единственная «чистая» immediate-инструкция с полным imm16.

### `LoadStaticInt` / `LoadStaticFloat` / `LoadStaticPointer`

```
dest = ST[op1]
```

Загружает значение из symbol table **по индексу** (не по SID). `LoadStaticInt` — i32/i64, `LoadStaticFloat` — f32, `LoadStaticPointer` — p64.

### `LoadStaticI8Imm` / `LoadStaticU8Imm` / `LoadStaticI16Imm` / `LoadStaticU16Imm`

```
dest = ST[op1]   (как i8/u8/i16/u16)
```

Загружает 64-битное значение из таблицы, но обрезает до указанной ширины.

### `LoadStaticI32Imm` / `LoadStaticU32Imm`

```
dest = ST[op1]   (как i32/u32)
```

`LoadStaticU32Imm` для UC4 дополнительно используется как загрузка `sid32` (см. `disassembler.cpp`, `process_instruction`).

### `LoadStaticI64Imm` / `LoadStaticU64Imm`

```
dest = ST[op1]   (как i64/u64)
```

`LoadStaticU64Imm` часто используется для загрузки SID64 или строковых указателей.

### `LoadStaticFloatImm`

```
dest = ST[op1]   (как f32)
```

### `LoadStaticPointerImm`

```
dest = ST[op1]   (как p64)
```

В декомпиляторе, если значение указывает в строковую секцию — интерпретируется как `"строка"`.

---

## 8. Загрузка из памяти (разыменование)

Все `Load*` — это `dest = *(T*)op1`, где `T` — тип.

| Мнемоника | Ширина | Знаковость |
| --- | --- | --- |
| `LoadI8` | 1 байт | знаковый |
| `LoadU8` | 1 байт | беззнаковый |
| `LoadI16` | 2 байта | знаковый |
| `LoadU16` | 2 байта | беззнаковый |
| `LoadI32` | 4 байта | знаковый |
| `LoadU32` | 4 байта | беззнаковый |
| `LoadI64` | 8 байт | знаковый |
| `LoadU64` | 8 байт | беззнаковый |
| `LoadFloat` | 4 байта | f32 |
| `LoadPointer` | 8 байт | p64 |

**Особенность `LoadPointer`**: в интерпретаторе (`parse_instruction.cpp`) он объединён с `LoadI64`/`LoadU64` в одну ветку `LABEL_110`:

```
*(u64*)_RDI[_RSI] = _RDI[_R15]   // если это Store*
_RDI[_RBP] = *(u64*)_RDI[_RSI]
```

То есть `LoadI64`, `LoadU64`, `LoadPointer` — одна и та же операция на уровне VM, различие только в типе, который отслеживает декомпилятор.

---

## 9. Запись в память

Все `Store*` — это `*(T*)op1 = op2`. Возвращают `op2` (значение) в `dest` — это позволяет использовать store как выражение-присваивание.

| Мнемоника | Ширина | Примечание |
| --- | --- | --- |
| `StoreI8` | 1 байт | знаковый |
| `StoreU8` | 1 байт | беззнаковый |
| `StoreI16` | 2 байта | знаковый |
| `StoreU16` | 2 байта | беззнаковый |
| `StoreI32` | 4 байта | знаковый |
| `StoreU32` | 4 байта | беззнаковый |
| `StoreI64` | 8 байт | знаковый |
| `StoreU64` | 8 байт | беззнаковый |
| `StoreInt` | 4 байта | алиас `StoreI32` |
| `StoreFloat` | 4 байта | f32 |
| `StorePointer` | 8 байт | p64 |
| `StoreArray` | 16 байт | копия xmmword |

**Особенность `StoreFloat`**: интерпретатор проверяет на NaN/Inf:

```
if ((v104 & 0x7F800000) == 0x7F800000) error("script_set_float_error");
```

и только потом пишет.

**Особенность `StoreArray`**: копирует 16 байт через `vmovups`, возвращает `op2` в `dest`. Используется в `breakpoint::emit_dc` (`StoreArray 00, 0x69, 0x69`) как отладочная заглушка.

---

## 10. Symbol table и вызовы

### `LookupInt` / `LookupFloat` / `LookupPointer`

```
dest = lookup(ST[op1])
```

**Ключевое отличие от `LoadStatic*`:** `LoadStatic*` берёт значение **напрямую** из таблицы, а `Lookup*` **резолвит** его через внешний lookup-механизм (функция `lookup` по адресу `0x141356D60`). Это используется для SID → строка/значение.

Поведение:

1. `v65 = lookup(symbol_table_ptr[op1])` — резолвит SID в указатель на значение.
2. Если `v65 != nullptr` — `dest = *v65`, иначе `dest = 0`.
3. В `v100[2*RBP]` сохраняется `symbol_table_ptr[op1]` — это SID для последующего использования.

**Особенность в декомпиляторе:** `pointer_gets_called(dest, istr_idx+1, fn)` проверяет, вызывается ли результат `LookupPointer` через `Call`/`CallFf`. Если да — тип становится `function_type`, иначе — `ptr_type`. Это критично для корректной сигнатуры функций.

### `Call` — ближний вызов

```
dest = op1(args...)   ; op2 = количество аргументов
```

1. `v105 = _RDI[op1]` — адрес функции.
2. Если `v105 == 0` — ошибка "Unable to call defun".
3. Выделяется новый stack frame, копируются `op2` аргументов из `r49...`.
4. Сохраняется `m_entry` (текущий instruction_idx_ptr) для возврата.
5. Вызывается `sub_141238750(6, 0xC9E40B98FED629EB, frame)` — это маркер/магия.
6. После возврата — восстановление frame, `dest = возвращаемое значение`.

### `CallFf` — дальний вызов

```
dest = op1(args...)   ; op2 = количество аргументов
```

1. `v73 = _RDI[op1]` — указатель на таблицу функций.
2. Копирует 49+ аргументов в локальный буфер (`v108`).
3. Вызывает `(**v73)(v73, v108, op2, &_RDI[dest], v73)` — передаёт vtable-стиль.
4. Проверяет `v107` (return code): если `1` — `*a2 = 0` и выход; если `2` — продолжение.
5. Освобождает stack frame.

**Отличие от `Call`:** `Call` использует прямой адрес функции, `CallFf` — через таблицу (far call). В декомпиляторе `m_isFarCall` устанавливается через `using #name as far (args) -> ret`.

### `LoadParamCnt` — число параметров

```
dest = instruction_idx_ptr[1]
```

Загружает количество аргументов текущего вызова. Используется внутри функций для variadic-логики.

---

## 11. Управление потоком

### `Branch` — безусловный переход

```
target = dest | (op2 << 8)
instruction_idx_ptr = target
```

В интерпретаторе: `*instruction_idx_ptr = _RBP | (_R15 << 8)`. В дизассемблере есть оптимизация: если цель — тоже `Branch`, происходит «склейка» переходов (см. `process_instruction`, case `Branch`).

### `BranchIf` — переход, если true

```
if (_RDI[op1] != 0) target = dest | (op2 << 8)
```

Используется для `if`, `while`, `&&`, `||`.

### `BranchIfNot` — переход, если false

```
if (_RDI[op1] == 0) target = dest | (op2 << 8)
```

**Особенность дизассемблера:** в `process_instruction` для `BranchIf`/`BranchIfNot` есть логика «склейки» цепочек:

- Если цель — такая же условная ветка с тем же `op1` — переход «проваливается» дальше.
- Если цель — `BranchIf` с тем же `op1`, а текущая — `BranchIfNot` (и наоборот) — цель сдвигается на `+1`.
- Если цель — `OpLogNot` с тем же `op1` — анализируется следующая инструкция и т.д.

Это нужно для восстановления сложных логических условий (`&&`/`||`) при декомпиляции.

### `Return` — возврат

```
return dest
```

1. Если `!m_isScriptFunction` — вызывается `insert_return(dest)`, который добавляет `ast::return_stmt` с выражением из `m_transformableExpressions[dest]`.
2. В интерпретаторе: `LABEL_158` → `sub_1414BBE30(instruction_idx_ptr, a2, v13, v19)` — восстановление родительского frame.
3. Если `v85 == 0` — выход из функции (`LABEL_167`).
4. Иначе — восстановление `symbol_table_ptr`, `instruction_ptr`, `_RDI`, `v100` из нового frame.

**Особенность кодогенератора** (`function_definition::emit_dc`): если последняя инструкция — `Branch` (заглушка для return), она заменяется на `Return`, а все `m_returnBranchLocations` патчатся на эту позицию.

### `AssertPointer` — проверка указателя

```
assert(dest != nullptr)
```

В интерпретаторе — `continue` (no-op). В декомпиляторе устанавливает тип `dest` в `ptr_type{}`.

### `BreakFlag` — флаг прерывания

Служебная инструкция, в `parse_instruction.cpp` не обрабатывается (попадает в `default`). В дизассемблере пропускается.

---

## 12. Специальные и редкие

### `StoreArray` — копирование 16 байт

```
*(xmmword*)op1 = *(xmmword*)op2
dest = op2
```

Используется в `breakpoint` (`StoreArray 00, 0x69, 0x69` — магическая заглушка).

### `LoadParamCnt` — количество параметров

```
dest = instruction_idx_ptr[1]
```

### `BreakFlag`

Служебная инструкция, не обрабатывается интерпретатором.

---

## 13. Сводная таблица по категориям

| Категория | Мнемоники |
| --- | --- |
| Целочисленная арифметика | `IAdd`, `ISub`, `IMul`, `IDiv`, `IMod`, `INeg`, `IAbs`, `IAddImm`, `ISubImm`, `IMulImm`, `IDivImm`, `IntAsh` |
| Плавающая арифметика | `FAdd`, `FSub`, `FMul`, `FDiv`, `FMod`, `FNeg`, `FAbs` |
| Сравнения (целые) | `IEqual`, `INotEqual`, `IGreaterThan`, `IGreaterThanEqual`, `ILessThan`, `ILessThanEqual` |
| Сравнения (float) | `FEqual`, `FNotEqual`, `FGreaterThan`, `FGreaterThanEqual`, `FLessThan`, `FLessThanEqual` |
| Логические | `OpLogAnd`, `OpLogOr`, `OpLogNot` |
| Битовые | `OpBitAnd`, `OpBitOr`, `OpBitXor`, `OpBitNot`, `OpBitNor` |
| Пересылки | `Move`, `MoveInt`, `MoveFloat`, `MovePointer` |
| Приведение | `CastInteger`, `CastFloat` |
| Константы | `LoadU16Imm`, `LoadStatic*`, `LoadStatic*Imm` |
| Загрузка | `LoadI8`, `LoadU8`, `LoadI16`, `LoadU16`, `LoadI32`, `LoadU32`, `LoadI64`, `LoadU64`, `LoadFloat`, `LoadPointer` |
| Запись | `StoreI8`, `StoreU8`, `StoreI16`, `StoreU16`, `StoreI32`, `StoreU32`, `StoreI64`, `StoreU64`, `StoreInt`, `StoreFloat`, `StorePointer`, `StoreArray` |
| Symbol table | `LookupInt`, `LookupFloat`, `LookupPointer` |
| Вызовы | `Call`, `CallFf`, `LoadParamCnt` |
| Управление | `Branch`, `BranchIf`, `BranchIfNot`, `Return`, `AssertPointer`, `BreakFlag` |

---

## 14. Практические заметки для реверса

1. **`LookupPointer` vs `LoadStaticPointer`**: первый резолвит SID в значение через внешний lookup (может вернуть 0), второй — берёт значение из таблицы «как есть».
2. **`Call` vs `CallFf`**: `Call` — прямой вызов (near), `CallFf` — через таблицу (far). При декомпиляции `m_isFarCall` влияет на выбор опкода.
3. **`BranchIf`/`BranchIfNot` цепочки**: дизассемблер специально «склеивает» их, чтобы восстановить `&&`/`||`. Это одна из самых сложных частей декомпилятора.
4. **`Store*` возвращает значение**: это позволяет использовать `a = b = c` в исходнике — присваивание как выражение.
5. **`Move` копирует всё**: значение, тип, offset, признак аргумента. `MoveInt`/`MoveFloat`/`MovePointer` — только значение с явным типом.
6. **`CastInteger`/`CastFloat` — in-place**: работают с `dest` как источником и приёмником. Это значит, что перед ними всегда должен быть `Move` или загрузка.
7. **`LoadU16Imm`** — единственная инструкция с полноценным imm16. Все остальные immediate — через symbol table или `op2` как imm8/imm16.
8. **NaN в `StoreFloat`** — проверяется и логируется, но запись всё равно происходит (в коде нет `return` после `error`).
9. **`OpBitNor`** — редко встречается, обычно результат оптимизации `~(a | b)`.
10. **`IntAsh`** — знаковый сдвиг: отрицательный `op2` = вправо, положительный = влево. В C++ это `a << b` или `a >> -b`.

Если нужно — могу сделать машиночитаемый CSV/JSON или добавить примеры дизассемблированного кода для каждой категории.
