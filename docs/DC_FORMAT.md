# DC_FORMAT.md — Полная документация формата DC-файлов

**Версия:** 2.0 (sid32 / UC4-T1X канон)
**Для кого:** для тех, кто впервые видит DC-файлы и хочет понять, как они устроены
**Стиль:** от простого к сложному, с примерами и hexdump-ами

---

## Оглавление

1. [Введение — что такое DC-файл](#1-введение)
2. [Базовые понятия — SID, offset, relocatable](#2-базовые-понятия)
3. [Общая структура файла](#3-общая-структура-файла)
4. [DC_Header — заголовок файла](#4-dc_header)
5. [DCEntry — таблица записей](#5-dcentry)
6. [ScriptLambda — байткод функции](#6-scriptlambda)
7. [StateScript — конечный автомат](#7-statescript)
8. [SsDeclarationList и SsDeclaration — объявления переменных](#8-ssdeclarationlist)
9. [SsOptions и SymbolArray — опции](#9-ssoptions)
10. [SsState — состояния](#10-ssstate)
11. [SsOnBlock — блоки состояний](#11-ssonblock)
12. [SsTrackGroup и SsTrack — треки](#12-sstrackgroup)
13. [SsLambda — лямбды в треках](#13-sslambda)
14. [Reloc-таблица](#14-reloc-таблица)
15. [String table — строки](#15-string-table)
16. [Полный пример — ss-lab-work-light.bin](#16-полный-пример)
17. [Ловушки и подводные камни](#17-ловушки)

---

## 1. Введение

### Что такое DC-файл

**DC** (DianaC? — точное значение неизвестно) — это **скриптовый формат** Naughty Dog. Используется в:

- **The Last of Us Part II** (T2R)
- **Uncharted 4** (UC4)
- **Uncharted: Lost Legacy** (T1X)

DC-файлы лежат в папке `dc1/` игры. Внутри — **байткод** для **виртуальной машины** (VM), которая исполняет скриптовую логику игры: состояния NPC, скрипты уровней, поведение оружия, диалоги и т.д.

### Зачем это знать

Если вы:
- **модифицируете** игру — нужно понимать, где лежат данные для редактирования;
- **реверсите** — нужно знать формат;
- **пишете инструменты** — нужно точно знать структуры.

### Три формата

| Формат | Игра | Инструкция | SID | sizeof(ScriptLambda) |
|---|---|---|---|---|
| **T2R** | TLOU2 | 8 байт (`LongInstruction`) | 64 бита | 0x58 |
| **UC4** | Uncharted 4 | 4 байта (`ShortInstruction`) | 32 бита | 0x50 |
| **T1X** | Lost Legacy | 4 байта | 32 бита | 0x50 |

**В этом документе описан sid32-канон (UC4/T1X)**, с пометками для T2R.

---

## 2. Базовые понятия

### SID — String ID

**SID** (String ID) — это **числовой хеш строки**. Вместо строки в файле хранится **число** (4 или 8 байт).

**Зачем:** экономия места, быстрое сравнение.

**Пример:**
```c
// В исходном коде:
foo("get-object-position");

// В файле:
0x54451733  // SID от "get-object-position"
```

**Обратное преобразование:** SID → строка **невозможно** без таблицы. Нужен внешний `sidbase.bin` или `sids.txt`.

**Алгоритм (sid32, CRC32):**
```python
def StringId(s):
    crc32("array" + s)
    crc32("(array" + s + ")")
    crc32("inline-array" + s)
    crc32("(inline-array" + s + ")")
    return crc32(s)
```

### Offset — смещение

**Offset** — это **смещение от начала файла** в байтах. Не путать с **абсолютным указателем** (адрес в памяти).

**Пример:**
```
0x368  // offset (смещение) — данные на 0x368 от начала файла
```

### Relocatable — перемещаемый

**Relocatable** — данные, которые при **загрузке** в память превращаются в **абсолютные указатели**.

**Пример:** в файле указатель хранится как **offset** (0x368). При загрузке добавляется **базовый адрес** (0x7FF600000000), получается **абсолютный указатель** (0x7FF600000368).

**Зачем:** файл можно загрузить **по любому адресу** — reloc-таблица «починит» указатели.

### Instruction — инструкция

**Инструкция** — 4 байта (sid32) или 8 байт (sid64):

| Байт | Поле |
|---|---|
| 0 | `opcode` (тип операции) |
| 1 | `destination` (регистр-приёмник) |
| 2 | `operand1` |
| 3 | `operand2` |

**Пример:**
```
15 00 00 00   lookupPointer r0, r0, r0
```

`0x15` = `lookupPointer`. `r0` — приёмник. `r0`, `r0` — операнды.

---

## 3. Общая структура файла

```
+0x00  ┌─────────────────────┐
       │   DC_Header (28)    │
+0x1C  ├─────────────────────┤
       │   DCEntry[0] (12)   │
       │   DCEntry[1] (12)   │
       │   ...               │
       │   DCEntry[N-1] (12) │
       ├─────────────────────┤
       │   Bodies            │  ← ScriptLambda / StateScript / data-instance
       │   (переменный       │
       │    размер)          │
       ├─────────────────────┤
       │   String table      │  ← нуль-терминированные строки
       ├─────────────────────┤
       │   Padding (0-3 байта)│
       ├─────────────────────┤
       │   Reloc table       │  ← bitmap указателей
       └─────────────────────┘
```

**Инварианты:**
- `m_stringsOffset < m_textSize`.
- Reloc-таблица лежит **ровно** по `m_textSize`.
- Всё **до** `m_textSize` — relocatable.
- Всё **после** `m_textSize` — **не** relocatable (только сама reloc-таблица).

---

## 4. DC_Header

### Описание

**DC_Header** — заголовок файла, **28 байт** (sid32).

### C++ структура

```c
struct DC_Header {          // 0x1C = 28 байт
    u32  m_magic;           // +0x00
    u32  m_versionNumber;   // +0x04
    u32  m_textSize;        // +0x08
    u32  m_stringsOffset;   // +0x0C
    u32  m_always1;         // +0x10
    u32  m_numEntries;      // +0x14
    u32  m_defTableOff;     // +0x18
};
```

### Поля

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_magic` | `u32` | Магия `0x44433030` = `"DC00"` (ASCII). Всегда. |
| `0x04` | 4 | `m_versionNumber` | `u32` | Версия формата. Обычно `1`. |
| `0x08` | 4 | `m_textSize` | `u32` | Размер всего relocatable-блока. Reloc-таблица начинается **ровно здесь**. |
| `0x0C` | 4 | `m_stringsOffset` | `u32` | Смещение string table от начала файла. Обычно `0` (нет строк) или **адрес** строк. |
| `0x10` | 4 | `m_always1` | `u32` | Всегда `1`. Семантика неизвестна. |
| `0x14` | 4 | `m_numEntries` | `u32` | Число записей в `DCEntry[]`. |
| `0x18` | 4 | `m_defTableOff` | `u32` | Смещение таблицы `DCEntry[]`. Обычно `0x1C` (сразу после заголовка). |

### Пример (hexdump)

```
0x00: 44 43 30 30   DC00                 ← magic
0x04: 00 00 00 01   version = 1
0x08: 00 00 42 00   textSize = 0x4200
0x0C: 00 00 00 00   stringsOffset = 0
0x10: 00 00 00 01   always1 = 1
0x14: 00 00 00 46   numEntries = 70
0x18: 00 00 00 1C   defTableOff = 0x1C
```

### Отличие T2R

В T2R заголовок **32 байта** (0x20), последнее поле — **8-байтный** указатель `m_pStartOfData`, а не 4-байтный offset.

---

## 5. DCEntry

### Описание

**DCEntry** — одна **запись** в таблице файла. Каждая запись = **один скрипт** (лямбда, state-script, или data-instance).

**Размер:** 12 байт (sid32), 24 байта (sid64).

### C++ структура

```c
struct DCEntry {            // 0xC = 12 байт (sid32)
    sid32 m_nameID;         // +0x00
    sid32 m_typeId;         // +0x04
    u32   m_entryPtr;       // +0x08 (offset)
};
```

### Поля

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_nameID` | `sid32` | SID имени записи (например, `SID("ss-lab-work-light")`). |
| `0x04` | 4 | `m_typeId` | `sid32` | SID **типа** payload: `SID("script-lambda")`, `SID("state-script")`, `SID("map")`, либо SID имени типа для data-instance. |
| `0x08` | 4 | `m_entryPtr` | `u32` | Смещение на payload. **Relocated** при загрузке. |

### Пример

```
0x1C: 5D 4D EB BF   m_nameID  = SID("ss-lab-work-light")
0x20: 45 C5 1F 9F   m_typeId  = SID("state-script")
0x24: 00 00 00 2C   m_entryPtr = 0x2C (offset на StateScript)
```

### Как читать

```python
fp.seek(0x1C)                     # начало таблицы
nameId   = read_u32(fp)           # SID имени
typeId   = read_u32(fp)           # SID типа
entryPtr = read_u32(fp)           # offset на payload
```

**Имя** — для отображения (`native_table.get(nameId)`).
**Тип** — для решения, как парсить payload.
**entryPtr** — куда идти за данными.

---

## 6. ScriptLambda

### Описание

**ScriptLambda** — **функция**. Содержит **байткод** и **symbol table**.

**Размер:** 24 байта (sid32, по нашей реверс-инженерии), 0x58 (T2R).

### C++ структура (sid32)

```c
struct ScriptLambda {           // 0x18 = 24 байта
    u32   m_pInstruction;       // +0x00 (offset на байткод)
    u32   m_pSymbols;           // +0x04 (offset на symbol table)
    sid32 m_typeId;             // +0x08 = SID("function")
    u32   m_sum;                // +0x0C (размер блока)
    sid32 m_funcName;           // +0x10 = 0 для анонимных
    sid32 m_type;               // +0x14 = SID("script-lambda")
};
```

### Поля

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_pInstruction` | `u32` | Смещение на **байткод**. `n` инструкций × 4 байта. **Relocated**. |
| `0x04` | 4 | `m_pSymbols` | `u32` | Смещение на **symbol table**. `m` символов × 4 байта. **Relocated**. |
| `0x08` | 4 | `m_typeId` | `sid32` | `SID("function")`. Всегда. |
| `0x0C` | 4 | `m_sum` | `u32` | Размер блока (в sid32 — **не** число инструкций). |
| `0x10` | 4 | `m_funcName` | `sid32` | SID имени функции; `0` — анонимная. |
| `0x14` | 4 | `m_type` | `sid32` | `SID("script-lambda")`. Всегда (обычно). |

### Что внутри

**Байткод** — от `m_pInstruction` до `m_pSymbols`. `num_instr = (m_pSymbols - m_pInstruction) / 4`.

**Symbol table** — от `m_pSymbols` до следующей значимой структуры. Содержит **SID-ы** (первые `n`) и **литералы** (числа).

**Пример:**
```
ScriptLambda @0x05C4 {
  m_pInstruction = 0x0A7C
  m_pSymbols     = 0x0AB8
  m_typeId       = 0xAB3EB31F (function)
  m_type         = 0x9ED499E1 (script-lambda)
}

SymbolTable[2] {
  [0] 0x55D2738C sid: is-level-loaded?
  [1] 0x0B1B1B8A sid: tag-symbol
}

--- bytecode (15 instructions) ---
0x00000A7C: 15 00 00 00 lookupPointer r0, r0, r0
0x00000A80: 15 01 01 00 lookupPointer r1, r1, r0
...
```

### Отличие T2R

В T2R `ScriptLambda` — 0x58 байт, `m_pInstruction` и `m_pSymbols` — 8-байтные указатели, есть `m_numInstructions` и `m_numSymbols`.

---

## 7. StateScript

### Описание

**StateScript** — **конечный автомат**. Содержит состояния, блоки, треки и лямбды. Это «главная» структура для игровых скриптов.

**Размер:** 48 байт (sid32), 0x50 (T2R).

### C++ структура (sid32)

```c
struct StateScript {                // 0x30 = 48 байт
    sid32 m_stateScriptId;          // +0x00
    u32   m_pSsDeclList;            // +0x04 (offset)
    sid32 m_initialStateId;         // +0x08
    u32   m_pSsOptions;             // +0x0C (offset)
    u32   m_always0_1;              // +0x10
    u32   m_always0_2;              // +0x14
    u32   m_pSsStateTable;          // +0x18 (offset)
    u32   m_stateCount_line;        // +0x1C (2×u16)
    u32   m_pDebugFileName;         // +0x20 (offset)
    u32   m_pErrorName;             // +0x24 (offset или SID)
    u32   m_padding1;               // +0x28
    u32   m_unknown;                // +0x2C
};
```

### Поля

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_stateScriptId` | `sid32` | SID имени state-script. |
| `0x04` | 4 | `m_pSsDeclList` | `u32` | Смещение на `SsDeclarationList`. **Relocated**. Может быть 0. |
| `0x08` | 4 | `m_initialStateId` | `sid32` | SID начального состояния. Должно совпадать с одним из `SsState.m_stateId`. |
| `0x0C` | 4 | `m_pSsOptions` | `u32` | Смещение на `SsOptions`. **Relocated**. |
| `0x10` | 4 | `m_always0_1` | `u32` | Всегда 0. |
| `0x14` | 4 | `m_always0_2` | `u32` | Всегда 0. |
| `0x18` | 4 | `m_pSsStateTable` | `u32` | Смещение на `SsState[m_stateCount]`. **Relocated**. |
| `0x1C` | 2 | `m_stateCount` | `u16` | Число состояний. |
| `0x1E` | 2 | `m_line` | `u16` | Номер строки для debug. |
| `0x20` | 4 | `m_pDebugFileName` | `u32` | Смещение на строку пути (debug). **Relocated**. |
| `0x24` | 4 | `m_pErrorName` | `u32` | Смещение на строку ошибки. **Relocated**. |
| `0x28` | 4 | `m_padding1` | `u32` | Padding. |
| `0x2C` | 4 | `m_unknown` | `u32` | Неизвестное поле. |

### Пример

```
StateScript {
  m_stateScriptId  = 0x5D4DEBBF (?)
  m_pSsDeclList    = 0x0084
  m_initialStateId = 0x8E93AC28 (?)
  m_pSsOptions     = 0x005C
  m_pSsStateTable  = 0x0094
  m_stateCount     = 4
  m_line           = 8
  m_pDebugFileName = 0x0F2C
    "t1final/t1/src/game/script/ss-lab/ss-lab-work-light.dc"
  m_pErrorName     = 0x00000000
}
```

### Иерархия

```
StateScript
├── SsDeclarationList       (объявления переменных)
├── SsOptions               (опции)
└── SsState[m_stateCount]   (состояния)
    └── SsOnBlock[]         (блоки: start, event, update, ...)
        └── SsTrackGroup    (встроенный)
            └── SsTrack[]   (треки)
                └── SsLambda[]  (лямбды)
                    └── ScriptLambda  (байткод)
```

---

## 8. SsDeclarationList

### Описание

**SsDeclarationList** — список **объявлений переменных** state-script.

### C++ структура (sid32)

```c
struct SsDeclarationList {              // 0x0C = 12 байт
    u32 m_totalDeclarationSize;         // +0x00
    u32 m_numDeclarations;              // +0x04
    u32 m_pDeclarations;                // +0x08 (offset)
};

struct SsDeclaration {                  // 0x10 = 16 байт
    sid32 m_declId;                     // +0x00
    sid32 m_declTypeId;                 // +0x04
    u16   m_varSizeSum;                 // +0x08
    u16   m_isVar;                      // +0x0A
    u32   m_pDeclValue;                 // +0x0C (offset)
};
```

### Поля SsDeclarationList

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_totalDeclarationSize` | `u32` | Суммарный размер всех объявлений. |
| `0x04` | 4 | `m_numDeclarations` | `u32` | Число элементов. |
| `0x08` | 4 | `m_pDeclarations` | `u32` | Смещение на `SsDeclaration[]`. **Relocated**. |

### Поля SsDeclaration

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_declId` | `sid32` | SID имени переменной (например, `SID("#counter")`). |
| `0x04` | 4 | `m_declTypeId` | `sid32` | SID типа: `"int32"`, `"float"`, `"string"`, `"symbol"`, `"boolean"`. |
| `0x08` | 2 | `m_varSizeSum` | `u16` | **Накопительный** размер (сумма всех до и включая это). |
| `0x0A` | 2 | `m_isVar` | `u16` | 1 — переменная, 0 — иначе. |
| `0x0C` | 4 | `m_pDeclValue` | `u32` | Смещение на **начальное значение**. **Relocated**. Может быть 0. |

### Пример

```
SsDeclarationList {
  m_totalDeclarationSize = 16
  m_numDeclarations      = 10
  m_pDeclarations        = 0x0244
}

SsDeclaration[10] {
  [0] declId=0x4152AD2D (float)   typeId=0x0F182EC3 pDeclValue=0x02E8
  [1] declId=0xEC715CFF (boolean) typeId=0x8B4E76FF pDeclValue=0x02F0
  [2] declId=0xA52B73C2 (int32)   typeId=0xC7CB275C pDeclValue=0x02F8
  [3] declId=0xA868551B (int32)   typeId=0xC7CB275C pDeclValue=0x0300
  [4] declId=0x8C01D740 (symbol)  typeId=0xC4AB6121 pDeclValue=0x0308
  ...
}
```

### Ловушка

**`m_varSizeSum` — кумулятивный!** Для N переменных размер не `m_varSizeSum`, а `m_varSizeSum[N-1] - m_varSizeSum[N-2]`. Это позволяет быстро вычислить смещение переменной в общем пуле.

**`m_pDeclValue`** — указывает на **инициализирующее значение**, **не на саму переменную**. Переменная живёт в symbol table VM. При чтении значения нужно `*(T*)m_pDeclValue`, где `T` — тип по `m_declTypeId`.

---

## 9. SsOptions

### Описание

**SsOptions** — **опции** state-script (debug-строки, символы).

### C++ структура (sid32)

```c
struct SsOptions {                  // 0x28 = 40 байт
    u32 m_optionString;             // +0x00 (offset на строку)
    u32 m_unknownFlags;             // +0x04
    u32 m_always0_1;                // +0x08
    u32 m_pSymbolArray;             // +0x0C (offset)
    u32 m_symbolArray2;             // +0x10 (offset)
    u32 m_symbolArray3;             // +0x14 (offset)
    u32 m_symbolArray4;             // +0x18 (offset)
    u32 m_always5;                  // +0x1C
    u32 m_mostly0;                  // +0x20
    u32 m_mostly0Rarely1;           // +0x24
};

struct SymbolArray {                // 0x0C = 12 байт
    u32 m_numEntries;               // +0x00
    u32 m_unk;                      // +0x04
    u32 m_pSymbols;                 // +0x08 (offset на массив sid32)
};
```

### Поля SsOptions

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_optionString` | `u32` | Смещение на строку с опциями. **Relocated**. |
| `0x04` | 4 | `m_unknownFlags` | `u32` | Флаги. Обычно `1`. |
| `0x08` | 4 | `m_always0_1` | `u32` | Всегда 0. |
| `0x0C` | 4 | `m_pSymbolArray` | `u32` | Основной `SymbolArray`. **Relocated**. |
| `0x10` | 4 | `m_symbolArray2` | `u32` | Вторичный. Обычно 0. |
| `0x14` | 4 | `m_symbolArray3` | `u32` | Третичный. Обычно 0. |
| `0x18` | 4 | `m_symbolArray4` | `u32` | Четвертичный. Обычно 0. |
| `0x1C` | 4 | `m_always5` | `u32` | Обычно `5`. |
| `0x20` | 4 | `m_mostly0` | `u32` | Обычно 0. |
| `0x24` | 4 | `m_mostly0Rarely1` | `u32` | Обычно 0, редко 1. |

### Поля SymbolArray

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_numEntries` | `u32` | Число элементов. |
| `0x04` | 4 | `m_unk` | `u32` | Reserved. |
| `0x08` | 4 | `m_pSymbols` | `u32` | Смещение на массив `sid32[m_numEntries]`. **Relocated**. |

### Пример

```
SsOptions {
  m_optionString   = 0x0EF8
    "lights"
    "ss-lab-work-light turn-on-light (on (start))"
  m_unknownFlags   = 0x00000001
  m_pSymbolArray   = 0x0000
  m_symbolArray2   = 0x0000
  m_symbolArray3   = 0x00EC
  m_symbolArray4   = 0x0000
  m_always5        = 5
}
```

### Ловушка

`SsOptions` содержит **4 указателя** на `SymbolArray`. **Нельзя** считать их **массивом**. Это **отдельные** поля. В 99% случаев заполнен только `m_pSymbolArray`.

**`m_optionString`** — строка **не-SID**. Это debug-представление опций.

---

## 10. SsState

### Описание

**SsState** — одно **состояние** state-script.

### C++ структура (sid32)

```c
struct SsState {                    // 0x0C = 12 байт
    sid32 m_stateId;                // +0x00
    u32   m_numSsOnBlocks;          // +0x04
    u32   m_pSsOnBlocks;            // +0x08 (offset)
};
```

### Поля

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_stateId` | `sid32` | SID имени состояния. |
| `0x04` | 4 | `m_numSsOnBlocks` | `u32` | Число блоков `SsOnBlock[]`. |
| `0x08` | 4 | `m_pSsOnBlocks` | `u32` | Смещение на `SsOnBlock[]`. **Relocated**. |

### Пример

```
SsState[4] {
  [0] stateId=0x8E93AC28 numSsOnBlocks=1 pSsOnBlocks=0x00C8
  [1] stateId=0xCB22D767 numSsOnBlocks=4 pSsOnBlocks=0x00F8
  [2] stateId=0xF01C4C7A numSsOnBlocks=5 pSsOnBlocks=0x017C
  [3] stateId=0xF81D859C numSsOnBlocks=1 pSsOnBlocks=0x0220
}
```

### Ловушка

`m_numSsOnBlocks` в T2R — **i64** (8 байт). В sid32 — `u32`. При парсинге **кастовать** к `size_t`.

---

## 11. SsOnBlock

### Описание

**SsOnBlock** — **блок** внутри состояния. Определяет, что происходит при **входе** в состояние (`start`), при **событии** (`event`), **каждый кадр** (`update`).

### C++ структура (sid32)

```c
struct SsOnBlock {                  // 0x20 = 32 байта
    u32   m_unknown1;               // +0x00
    sid32 m_blockEventId;           // +0x04
    u32   m_blockType;              // +0x08
    u16   m_totalLambdaCount;       // +0x0C
    u16   m_numTracks;              // +0x0E
    u32   m_aTracks;                // +0x10 (offset)
    u32   m_name;                   // +0x14 (offset на строку)
    u32   m_always0_1;              // +0x18
    u32   m_always0_2;              // +0x1C
};
```

### Поля

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_unknown1` | `u32` | 0 или 2. Семантика неизвестна. |
| `0x04` | 4 | `m_blockEventId` | `sid32` | SID события (если `m_blockType == Event`). |
| `0x08` | 4 | `m_blockType` | `u32` | Тип блока. **НО** реальный тип определяется **по имени строки** (см. ниже). |
| `0x0C` | 2 | `m_totalLambdaCount` | `u16` | Число лямбд во всех треках. |
| `0x0E` | 2 | `m_numTracks` | `u16` | Число треков. |
| `0x10` | 4 | `m_aTracks` | `u32` | Смещение на `SsTrack[]`. **Relocated**. |
| `0x14` | 4 | `m_name` | `u32` | Смещение на строку имени блока (например, `"ss-lab-work-light initial (on (start))"`). **Relocated**. |
| `0x18` | 4 | `m_always0_1` | `u32` | Всегда 0. |
| `0x1C` | 4 | `m_always0_2` | `u32` | Всегда 0. |

### Определение типа блока

**Поле `m_blockType` ненадёжно** — часто 0. **Настоящий тип** определяется **по имени строки**:

| Строка содержит | Тип |
|---|---|
| `(on (start))` | `Start` |
| `(on (end))` | `End` |
| `(on (event))` | `Event` |
| `(on (update))` | `Update` |
| `(on (virtual))` | `Virtual` |

### Пример

```
SsOnBlock[1] {
  [0] eventId=0x00000000 (None) type=Start totalLambdas=4 numTracks=1
      aTracks=0x0388 name=0x0E48
      name="ss-lab-work-light initial (on (start))"
}
```

---

## 12. SsTrackGroup

### Описание

**SsTrackGroup** — **встроенная** структура в `SsOnBlock`. Содержит **треки**.

**В sid32** — часть `SsOnBlock` (не отдельная структура с указателем).

### C++ структура (sid32, встроена в SsOnBlock)

```c
struct SsTrackGroup {               // часть SsOnBlock
    // ... (см. SsOnBlock)
};
```

### Поля (из SsOnBlock)

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x0C` | 2 | `m_totalLambdaCount` | `u16` | Суммарное число лямбд во всех треках. |
| `0x0E` | 2 | `m_numTracks` | `u16` | Число треков. |
| `0x10` | 4 | `m_aTracks` | `u32` | Смещение на `SsTrack[]`. **Relocated**. |
| `0x14` | 4 | `m_name` | `u32` | Смещение на строку имени группы. **Relocated**. |

### Ловушка

`m_totalLambdaCount` **не равно** сумме по трекам **автоматически**. Это **заявленное** значение. Реальное число лямбд = **сумма** `SsTrack[i].m_totalLambdaCount`. Использовать **сумму по трекам**, а `m_totalLambdaCount` — только для валидации.

---

## 13. SsTrack

### Описание

**SsTrack** — **трек**. Содержит лямбды.

### C++ структура (sid32)

```c
struct SsTrack {                    // 0x10 = 16 байт
    sid32 m_trackId;                // +0x00
    u16   m_trackIdx;               // +0x04
    u16   m_totalLambdaCount;       // +0x06
    u32   m_pSsLambda;              // +0x08 (offset)
    sid32 m_type;                   // +0x0C = SID("array")
};
```

### Поля

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_trackId` | `sid32` | SID имени трека. |
| `0x04` | 2 | `m_trackIdx` | `u16` | Индекс трека внутри группы. |
| `0x06` | 2 | `m_totalLambdaCount` | `u16` | Число лямбд в треке. |
| `0x08` | 4 | `m_pSsLambda` | `u32` | Смещение на `SsLambda[]`. **Relocated**. |
| `0x0C` | 4 | `m_type` | `sid32` | `SID("array")` (обычно). |

### Пример

```
SsTrack[1] {
  [0] @0x0388 trackId=0x87F13BC2 trackIdx=0 totalLambdas=4
      pSsLambda=0x0474 type=0x1ED2E0A2 (array)
}
```

---

## 14. SsLambda

### Описание

**SsLambda** — **обёртка** над `ScriptLambda` с метаданными.

### C++ структура (sid32)

```c
struct SsLambda {                       // 0x08 = 8 байт
    u32 m_pScriptLambda;                // +0x00 (offset)
    u32 m_someSortOfCounter;            // +0x04
};
```

### Поля

| Offset | Size | Field | Тип | Назначение |
|---|---|---|---|---|
| `0x00` | 4 | `m_pScriptLambda` | `u32` | Смещение на `ScriptLambda`. **Relocated**. Может быть 0. |
| `0x04` | 4 | `m_someSortOfCounter` | `u32` | Монотонно растущий счётчик. Может иметь пропуски. |

### Пример

```
SsLambda[4] {
  [0] pScriptLambda=0x05C4 counter=33
  [1] pScriptLambda=0x0534 counter=34
  [2] pScriptLambda=0x051C counter=35
  [3] pScriptLambda=0x057C counter=39
}
```

### Ловушка

`SsLambda` — это **обёртка**. Реальный **байткод** — в `m_pScriptLambda` (→ `ScriptLambda`). Если `m_pScriptLambda == 0` — слот **пустой**.

---

## 15. Reloc-таблица

### Описание

**Reloc-таблица** — bitmap, указывающий, какие **8-байтные слоты** (в sid64) или **4-байтные** (в sid32) содержат **указатели**.

### Формат

```
+0x00: u32 table_size (в байтах)
+0x04: byte bitmap[table_size]
```

### Алгоритм декодирования

```c
for slot in [0, table_size * 8):
    if bitmap[slot / 8] & (1 << (slot % 8)):
        // slot — 8-байтный (или 4-байтный) слот
        value = *(u64*)(bytes + slot * 8)
        value += base_address
        *(u64*)(bytes + slot * 8) = value
```

### Размер

```
table_size = ceil(m_textSize / 64)   // в байтах
```

**НЕ** `m_textSize / 8`. Потому что **один бит = 8 байт** (или 4 байта в sid32), а **8 бит = 64 байта** (или 32 байта в sid32).

### Пример

```
Reloc table size: 124 bytes (114 slots)
```

`124` байта bitmap = `124 × 8 = 992` бита. `992 × 4 = 3968` байт **relocatable**. `m_textSize = 0xF64 = 3940`. **Приблизительно** совпадает.

---

## 16. String table

### Описание

**String table** — массив **нуль-терминированных** ASCII-строк.

### Формат

```
+0x00: "first string\0"
+0x0D: "second string\0"
+0x1B: "third\0"
...
```

### Особенности

- Строки **не имеют длины** — только `\0`.
- Порядок строк = порядок **первого появления** в теле файла.
- **Интернирование**: одинаковые строки хранятся **один раз**.
- Перед reloc-таблицей строка **выравнивается** до 4 байт (padding от 0 до 3 нулей).

### Ловушка

`BinaryFile::replace_newlines_in_stringtable` **модифицирует** string table (заменяет `\n` на пробел). Это делается **один раз** при загрузке.

---

## 17. Полный пример

### Файл `ss-lab-work-light.bin`

**Размер:** `0x0F64` = **3940 байт**.
**Entries:** 1 (`state-script`).

### Разбор

```
DC_Header:
  magic         = "DC00"
  version       = 1
  textSize      = 0x0F64
  stringsOffset = 0
  always1       = 1
  numEntries    = 1
  defTableOff   = 0x1C

DCEntry[0] @ 0x1C:
  nameID    = 0x5D4DEBBF (ss-lab-work-light)
  typeId    = 0x45C51F9F (state-script)
  entryPtr  = 0x2C

StateScript @ 0x2C:
  m_stateScriptId   = 0x5D4DEBBF
  m_pSsDeclList     = 0x84
  m_initialStateId  = 0x8E93AC28
  m_pSsOptions      = 0x5C
  m_pSsStateTable   = 0x94
  m_stateCount      = 4
  m_line            = 8
  m_pDebugFileName  = 0xF2C
    "t1final/t1/src/game/script/ss-lab/ss-lab-work-light.dc"
  m_pErrorName      = 0

SsDeclarationList @ 0x84:
  m_totalDeclarationSize = 16
  m_numDeclarations      = 10
  m_pDeclarations        = 0x244

SsDeclaration[10] @ 0x244:
  [0] declId=0x4152AD2D (float)   typeId=0x0F182EC3 pDeclValue=0x02E8
  [1] declId=0xEC715CFF (boolean) typeId=0x8B4E76FF pDeclValue=0x02F0
  ...

SsOptions @ 0x5C:
  m_optionString  = 0xEF8
    "lights"
  m_always5       = 5

SsState[4] @ 0x94:
  [0] stateId=0x8E93AC28 numSsOnBlocks=1 pSsOnBlocks=0xC8
  [1] stateId=0xCB22D767 numSsOnBlocks=4 pSsOnBlocks=0xF8
  [2] stateId=0xF01C4C7A numSsOnBlocks=5 pSsOnBlocks=0x17C
  [3] stateId=0xF81D859C numSsOnBlocks=1 pSsOnBlocks=0x220

SsOnBlock @ 0xC8 (для state[0]):
  m_blockEventId = 0
  m_blockType    = 0
  m_numTracks    = 1
  m_aTracks      = 0x388
  m_name         = 0xE48
    "ss-lab-work-light initial (on (start))"

SsTrack @ 0x388:
  m_trackId          = 0x87F13BC2
  m_trackIdx         = 0
  m_totalLambdaCount = 4
  m_pSsLambda        = 0x474

SsLambda[4] @ 0x474:
  [0] pScriptLambda=0x05C4 counter=33
  [1] pScriptLambda=0x0534 counter=34
  [2] pScriptLambda=0x051C counter=35
  [3] pScriptLambda=0x057C counter=39

ScriptLambda @ 0x5C4:
  m_pInstruction = 0xA7C
  m_pSymbols     = 0xAB8
  m_typeId       = 0xAB3EB31F (function)
  m_type         = 0x9ED499E1 (script-lambda)

SymbolTable[2] @ 0xAB8:
  [0] 0x55D2738C sid: is-level-loaded?
  [1] 0x0B1B1B8A sid: tag-symbol

Bytecode @ 0xA7C (15 instructions):
  0xA7C: lookupPointer r0, r0, r0
  0xA80: lookupPointer r1, r1, r0
  ...
  0xAB4: return r0, r0, r0
```

---

## 18. Ловушки

### 18.1. `DCEntry` размер

- **sid32:** 12 байт.
- **sid64:** 24 байта.

**Проверяйте** размер перед парсингом.

### 18.2. `m_textSize`

**Включает** заголовок. `m_textSize >= 0x1C` (sid32).

### 18.3. Reloc-таблица

**`table_size = ceil(m_textSize / 64)`** в байтах — **НЕ** `m_textSize / 8`.

### 18.4. `SsOnBlock.m_blockType`

**Ненадёжно.** Определяйте **тип по строке имени** (`(on (start))`, `(on (event))`).

### 18.5. `SsTrackGroup.m_totalLambdaCount`

**Не равно** сумме по трекам **автоматически**. Используйте **сумму** `SsTrack[i].m_totalLambdaCount`.

### 18.6. `SsDeclaration.m_varSizeSum`

**Кумулятивный**. Для N переменных — `m_varSizeSum[N-1] - m_varSizeSum[N-2]`.

### 18.7. `SsDeclaration.m_pDeclValue`

**Указывает** на инициализирующее значение. **Не сама переменная**. Читать: `*(T*)m_pDeclValue`.

### 18.8. `SsOptions.m_pSymbolArray`

**4 отдельных указателя**, **не** массив. Проверяйте **каждый** на `0`.

### 18.9. `ScriptLambda.m_sum`

**Не использовать** для вычисления размера. Число инструкций = `(m_pSymbols - m_pInstruction) / 4`.

### 18.10. SID resolution

**SID в файле** — **число**. Для отображения имени **нужен** `sidbase.bin` или `sids.txt`. **Без него** — только `0x...`.

### 18.11. String table

**Модифицируется** при загрузке (`\n` → ` `). **Интернирование** — одинаковые строки **один раз**.

### 18.12. Регистры VM

**Все** регистры VM — **64 бита** в sid64, **32 бита** в sid32. **Не** `sizeof(int)`.

---

## Заключение

**DC-формат** — это **релокатируемый контейнер** скриптов. **Иерархия**:

```
DC_Header
└── DCEntry[]
    ├── ScriptLambda       (функция)
    └── StateScript        (конечный автомат)
        ├── SsDeclarationList  (переменные)
        ├── SsOptions          (опции)
        └── SsState[]
            └── SsOnBlock[]
                └── SsTrackGroup
                    └── SsTrack[]
                        └── SsLambda[]
                            └── ScriptLambda  (байткод)
```

**Парсинг:**
1. Прочитать `DC_Header`.
2. Прочитать `DCEntry[]`.
3. Для каждой entry:
   - Если `script-lambda` — читать `ScriptLambda`.
   - Если `state-script` — читать `StateScript`, рекурсивно обходить.
   - Если другой тип — читать как data-instance.
4. Прочитать string table.
5. Прочитать reloc-таблицу.

**Для отображения имён** — нужен `sidbase.bin`.

**Для полного понимания** — нужен **exe** игры (там описания типов).