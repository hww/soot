# DC_FORMAT.md

Спецификация бинарного формата DC-файлов (T2R / T1X / UC4).

---

## 1. Overview

### 1.1. Purpose of the DC format

DC-файл — это relocatable контейнер, содержащий **entries** — одну или несколько «единиц скриптового кода». Каждая entry — это либо отдельная функция (`ScriptLambda`), либо конечный автомат (`StateScript`) со вложенными блоками, треками, лямбдами. Файл используется игровым движком для загрузки скриптовой логики: при загрузке reloc-таблица превращает файловые оффсеты в абсолютные указатели, после чего VM исполняет код через опкоды, описанные в `Opcodes.hpp` / `Instructions.hpp`.

DC-файлы бывают трёх форматов, отличающихся деталями:
- **T2R** (`The Last of Us Part II`) — 8-байтовые инструкции (`Instruction`), `ScriptLambda == 0x58`.
- **T1X** — близок к T2R, часть полей иначе.
- **UC4** (`Uncharted 4`) — 4-байтовые инструкции (`ShortInstruction`), `ScriptLambda == 0x50`.

В этом документе описан **T2R-канон**, с явными пометками там, где UC4/T1X отличаются.

### 1.2. Terms and conventions

| Термин | Значение |
|---|---|
| **File offset** | Смещение от начала файла в байтах. |
| **Relocatable** | Данные, которые при загрузке превращаются в абсолютные указатели. |
| **Symbol table (ST)** | Массив 64-битных значений внутри лямбды. |
| **SID** | String ID — 64-битный хеш строки. |
| **Entry** | Одна единица скриптового кода (`ScriptLambda` или `StateScript`). |
| **Pointer** | 64-битное значение, которое reloc-таблица превращает в адрес. |
| **Body** | Сериализованные данные одной entry. |

Соглашения для таблиц:
- Все оффсеты — в hex, если не указано иное.
- `sizeof(X)` — размер структуры в байтах.
- `[N]` — массив из `N` элементов.

### 1.3. File layout at a glance

```
0x00                       DC_Header               (0x20 байт)
0x20                       SID("array")            (8 байт) — маркер массива entries
0x28                       DCEntry[m_numEntries]   (0x18 байт × N)
0x28 + 0x18*N              bodies                  (ScriptLambda / StateScript и вложенные)
m_stringsOffset            string table            (нуль-терминированные ASCII-строки)
m_textSize                 relocation table        (u32 size + bitmap)
```

Инварианты:
- `m_stringsOffset < m_textSize`.
- Relocation table лежит ровно по оффсету `m_textSize`.
- Всё до `m_textSize` — relocatable.
- Всё после `m_textSize` — не relocatable (только сама reloc-таблица).

---

## 2. File-level structures

### 2.1. DC_Header (0x20 байт)

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 4 | `m_magic` | Всегда `0x44433030` ("DC00"). |
| 0x04 | 4 | `m_versionNumber` | Всегда `0x1`. |
| 0x08 | 4 | `m_textSize` | Размер **всего relocatable-блока** (header + entries + bodies + strings + padding). Reloc-таблица начинается ровно здесь. |
| 0x0C | 4 | `m_stringsOffset` | Абсолютное смещение string table от начала файла. Равно `data_size` (header + entries + bodies). |
| 0x10 | 4 | `m_always1` | Всегда 1. Семантика неясна. |
| 0x14 | 4 | `m_numEntries` | Число записей в entry-таблице. |
| 0x18 | 8 | `m_pStartOfData` | Указатель на `Entry[0]`. **Relocated** при загрузке. Несмотря на имя, указывает на таблицу entries, не на «данные». |

Важно: `m_textSize` **включает** header, поэтому `m_textSize >= 0x20`.

### 2.2. DCEntry (0x18 байт)

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 8 | `m_nameID` | SID64 имени entry (например, `SID("#7C28D25188889230")`). |
| 0x08 | 8 | `m_typeId` | SID64 типа payload: `SID("script-lambda")` или `SID("state-script")`. |
| 0x10 | 8 | `m_entryPtr` | Указатель на payload. **Relocated**. |

`DCEntry[m_numEntries]` предваряется `SID("array")` (см. ниже). Первая entry — по оффсету `0x28`.

### 2.3. SID("array") marker (8 байт)

Перед любым массивом в DC-формате идёт `sid64 = SID("array")`. Это маркер, который загрузчик использует, чтобы отличить массив от структуры. В частности:

- Перед `DCEntry[]` — `SID("array")` по оффсету `0x20`.
- Перед `SsState[]`, `SsOnBlock[]`, `SsTrack[]`, `SsLambda[]`, `SymbolArray.m_pSymbols[]` — тоже `SID("array")` (внутри соответствующего блока).

**Ловушка:** маркер — не padding. При парсинге структуры `binary_file_inspector` проверяет `next_struct_header.get<sid64>() == SID("array")`, чтобы решить, массив это или struct. Если маркера нет — структура.

### 2.4. String table

Расположение: от `m_stringsOffset` до `m_textSize` (минус padding). Формат: последовательность нуль-терминированных ASCII-строк, подряд, без выравнивания.

- Строки не имеют длины — только нуль-терминатор.
- Порядок строк в таблице совпадает с порядком **первого появления** строки в теле файла (при кодогенерации).
- Все оффсеты строк — **относительные** (`offset` = сдвиг от `m_stringsOffset`), но при загрузке превращаются в абсолютные указатели через reloc (потому что строковые указатели в телах — relocated).
- Перед reloc-таблицей string table выравнивается до 4 байт (padding от 0 до 3 нулей).

**Ловушка:** string table может содержать `\n`; загрузчик (`BinaryFile::replace_newlines_in_stringtable`) заменяет их на пробелы, потому что игра так делает. Это **модифицирует** содержимое mapped-файла — при сохранении строки уже изменены.

### 2.5. Relocation table

Расположение: ровно по оффсету `m_textSize`. Формат:
- Первые 4 байта — `u32 table_size` (число **байтов** bitmap).
- Далее `table_size` байт — bitmap.

Bitmap: **один бит на каждые 8 байт** relocatable-блока. Бит `N` (в порядке LSB-first внутри байта) относится к 8-байтовому слоту с индексом `N`, то есть к оффсету `N * 8`.

Алгоритм декодирования (`BinaryFile::read_reloc_table`):
```
for slot in [0, table_size * 8):
    if bitmap[slot / 8] & (1 << (slot % 8)):
        value = *(u64*)(bytes + slot * 8)
        value += base_address
        *(u64*)(bytes + slot * 8) = value
```

После декодирования:
- Все указатели — абсолютные адреса в mapped-буфере.
- Параллельно строится `m_pointedAtTable` — bitmap «какие оффсеты являются целями указателей». Нужна для различения структур и указателей при дампе.

**Ловушка:** размер bitmap — `ceil(relocatable_size / 64)` **байт**, потому что один бит = 8 байт, а 8 бит = 64 байта. То есть:

```
table_size = ceil(m_textSize / 64)
```

Это **не** `m_textSize / 8`.

---

## 3. Script payloads

### 3.1. ScriptLambda (0x58 байт в T2R; 0x50 в UC4)

Одна лямбда = одна функция. Заголовок фиксированного размера:

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 8 | `m_type` | SID типа. В кодогенераторе пишется `SID("script-lambda")`. Документация icemesh предполагает value-type SID, но фактически там всегда `script-lambda`. |
| 0x08 | 8 | `m_pInstruction` | Указатель на `Instruction[m_numInstructions]`. **Relocated**. |
| 0x10 | 8 | `m_pSymbols` | Указатель на `u64[m_numSymbols]`. **Relocated**. |
| 0x18 | 8 | `m_typeId` | SID типа контейнера: `SID("script-lambda")`. |
| 0x20 | 8 | `m_sum` | Размер блока: `12 + 4 * (m_numInstructions + m_numSymbols)` (формула из `get_scriptlambda_sum`). Несмотря на имя «sum», это не сумма байтов, а в 32-битных единицах. |
| 0x28 | 8 | `m_funcName` | SID имени функции; `0` для анонимных (embedded) лямбд. |
| 0x30 | 8 | `m_instructionFlag` | Магия `0xDEADBEEF1337F00D`. |
| 0x38 | 4 | `m_always0_2` | Reserved, всегда 0. |
| 0x3C | 4 | `m_numInstructions` | Число инструкций. |
| 0x40 | 4 | `m_numSymbols` | Число символов в ST. **notcanonical** — не в icemesh-каноне, но присутствует в текущем формате. |
| 0x44 | 4 | `m_neg` | Всегда `-1`. Сигнатура/маркер конца. |
| 0x48 | 8 | `m_sidGlobal` | SID("global") для top-level функций; иначе SID scope'а. |
| 0x50 | 8 | `m_always0_3` | Reserved, всегда 0. **notcanonical** — не в icemesh-каноне. |

Итого: `sizeof(ScriptLambda) == 0x58` в текущем формате carbon.

Инструкции (по `m_pInstruction`) идут **сразу после** заголовка, плотно: `m_numInstructions * sizeof(Instruction)` байт. Затем — symbol table: `m_numSymbols * 8` байт.

**Ловушка:** `m_numSymbols` отсутствует в icemesh-каноне. В агрегатном инициализаторе `function.cpp` (dconstruct) **пропущено** — из-за этого `m_neg` в исходном коде инициализируется значением `-1`, а `m_sidGlobal` — значением `global_sid`. Если у тебя структура **не** совпадает с этой раскладкой — размер не 0x58 и парсинг едет.

### 3.2. StateScript (0x50 байт)

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 8 | `m_stateScriptId` | SID64 имени state-script. |
| 0x08 | 8 | `m_pSsDeclList` | Указатель на `SsDeclarationList`. **Relocated**. Может быть `nullptr`. |
| 0x10 | 8 | `m_initialStateId` | SID64 начального состояния. |
| 0x18 | 8 | `m_pSsOptions` | Указатель на `SsOptions`. **Relocated**. Может быть `nullptr`. |
| 0x20 | 8 | `m_always0_1` | Reserved, всегда 0. |
| 0x28 | 8 | `m_pSsStateTable` | Указатель на `SsState[m_stateCount]`. **Relocated**. |
| 0x30 | 2 | `m_stateCount` | Число состояний. |
| 0x32 | 2 | `m_line` | Номер строки для debug overlay. |
| 0x34 | 4 | `m_always0_2` | Reserved, всегда 0. |
| 0x38 | 8 | `m_pDebugFileName` | Строка пути (например, `"t2r/src/game/scriptx/ss/ss-tag-as-hero.dcx"`). **Relocated**. Может быть `nullptr`. |
| 0x40 | 8 | `m_pErrorName` | Строка ошибки. **Relocated**. Может быть `nullptr`. |
| 0x48 | 8 | `m_padding` | Reserved, всегда 0. |

**Ловушка:** `m_stateCount` — это `i16` (signed), но на практике всегда положительное. Используется как размер массива `m_pSsStateTable`. **Размер массива определяется только этим полем** — никакого размера в самих `SsState` нет.

### 3.3. SsDeclarationList (0x10 байт) / SsDeclaration (0x30 байт)

**SsDeclarationList:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 4 | `m_totalDeclarationSize` | Суммарный размер всех объявлений в байтах. |
| 0x04 | 4 | `m_numDeclarations` | Число элементов в `m_pDeclarations`. |
| 0x08 | 8 | `m_pDeclarations` | Указатель на `SsDeclaration[m_numDeclarations]`. **Relocated**. |

**SsDeclaration:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 8 | `m_declId` | SID64 имени переменной (например, `SID("#health")`). |
| 0x08 | 8 | `m_declIdString` | Указатель на строку имени. **Relocated**. Может быть `nullptr`. |
| 0x10 | 8 | `m_declTypeId` | SID64 типа: `"int32"`, `"float"`, `"string"`, `"symbol"`, `"boolean"`. |
| 0x18 | 2 | `m_varSizeSum` | **Накопительный** размер: сумма размеров всех объявлений **до и включая** это. Позволяет вычислить смещение в общем пуле значений. |
| 0x1A | 2 | `m_isVar` | 1 если переменная, 0 иначе. |
| 0x1C | 4 | `m_always0` | Reserved. |
| 0x20 | 8 | `m_pDeclValue` | Указатель на начальное значение в data-сегменте. **Relocated**. Может быть `nullptr`. |
| 0x28 | 8 | `m_always0x80` | В норме `0x80`. Семантика неясна. |

**Ловушка:** `m_varSizeSum` — кумулятивный. Для N переменных размер — не `m_varSizeSum`, а `m_varSizeSum[N-1] - m_varSizeSum[N-2]`. Если поле маленькое (например, `u16`), при большом количестве переменных возможно переполнение.

**Ловушка:** `m_pDeclValue` указывает на **инициализирующее значение**. Это не сама переменная — переменная живёт в symbol table VM. При чтении значения нужно: `*(T*)m_pDeclValue`, где `T` — тип по `m_declTypeId`.

### 3.4. SsOptions (0x50 байт) / SymbolArray (0x10 байт)

**SsOptions:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 8 | `m_optionString` | Строка с опциями (debug). **Relocated**. Может быть `nullptr`. |
| 0x08 | 8 | `m_unknownFlags` | Флаги, семантика неясна. |
| 0x10 | 8 | `m_always0_1` | Reserved. |
| 0x18 | 8 | `m_pSymbolArray` | Основной `SymbolArray`. **Relocated**. Может быть `nullptr`. |
| 0x20 | 8 | `m_symbolArray2` | Вторичный `SymbolArray`. Почти всегда `nullptr`. |
| 0x28 | 8 | `m_symbolArray3` | Третичный. Почти всегда `nullptr`. |
| 0x30 | 8 | `m_symbolArray4` | Четвертичный. Почти всегда `nullptr`. |
| 0x38 | 4 | `m_always5` | В норме `5`. |
| 0x3C | 4 | `m_mostly0` | Обычно 0; иногда `16, 32, 1, 13, 21, 8`. |
| 0x40 | 8 | `m_mostly0Rarely1` | Обычно 0, редко 1. |
| 0x48 | 8 | `m_always0_2` | Reserved. |

**SymbolArray:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 4 | `m_numEntries` | Число элементов в `m_pSymbols`. |
| 0x04 | 4 | `m_unk` | Reserved. |
| 0x08 | 8 | `m_pSymbols` | Указатель на `sid64[m_numEntries]`. **Relocated**. |

**Ловушка SsOptions:** `m_pSymbolArray` — это по сути список опций («opts»). Но у него 4 поля (`m_pSymbolArray`, `m_symbolArray2/3/4`). В 99% случаев заполнен только первый, остальные — `nullptr`. При парсинге **нельзя** предполагать, что там массив SymbolArray[4] — это **четыре отдельных указателя** на отдельные объекты. Если все четыре `nullptr` — options пуст.

**Ловушка SymbolArray:** `m_pSymbols` — это **указатель на массив `sid64`**, не на строки. Строки получаются через `StringIdManager::get_string(sid)`. Это отличается от `SsDeclarationList::m_pDeclarations`, где элементы — структуры.

### 3.5. SsState (0x18 байт)

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 8 | `m_stateId` | SID64 имени состояния. |
| 0x08 | 8 | `m_numSsOnBlocks` | Число элементов в `m_pSsOnBlocks`. |
| 0x10 | 8 | `m_pSsOnBlocks` | Указатель на `SsOnBlock[m_numSsOnBlocks]`. **Relocated**. |

**Ловушка:** `m_numSsOnBlocks` — `i64`, но используется как размер. При парсинге кастуй к `size_t`.

### 3.6. SsOnBlock (0x50 байт) / SsTrackGroup (0x38 байт)

**SsOnBlock:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 4 | `m_blockType` | `0=Start, 1=End, 2=Event, 3=Update, 4=Virtual, 5=Code, 6=Exit, 7=Post`. |
| 0x04 | 4 | `m_always0` | Reserved. |
| 0x08 | 8 | `m_blockEventId` | SID64 имени события. Имеет смысл только при `m_blockType == Event`; иначе может быть 0. |
| 0x10 | 8 | `m_pScriptLambda` | Указатель на `ScriptLambda`. **Relocated**. Может быть `nullptr`. |
| 0x18 | 0x38 | `m_trackGroup` | **Встроенный** `SsTrackGroup`. |

**SsTrackGroup:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 8 | `m_always0` | Reserved. |
| 0x08 | 2 | `m_totalLambdaCount` | **Суммарное** число лямбд во **всех** треках этой группы. |
| 0x0A | 2 | `m_numTracks` | Число элементов в `m_aTracks`. |
| 0x0C | 4 | `m_padding` | Reserved. |
| 0x10 | 8 | `m_aTracks` | Указатель на `SsTrack[m_numTracks]`. **Relocated**. |
| 0x18 | 8 | `m_name` | Строка debug-имени (`"ss-fp-test initial (on (start))"`). **Relocated**. |
| 0x20 | 8 | `m_always0_1` | Reserved. |
| 0x28 | 8 | `m_always0_2` | Reserved. |
| 0x30 | 8 | `m_rareScriptLambda` | Указатель на редкий `ScriptLambda`. **Relocated**. Почти всегда `nullptr`. |

**Ловушка (важная):** `m_totalLambdaCount` **не равно** сумме по трекам автоматически. Это **заявленное** значение, а реальное число лямбд вычисляется как сумма `SsTrack[i].m_totalLambdaCount` для `i in [0, m_numTracks)`. Если они не совпадают — файл повреждён. Использовать нужно **сумму по трекам**, а `m_totalLambdaCount` — только для валидации.

**Ловушка:** `m_rareScriptLambda` — отдельный указатель **рядом** с `m_aTracks`. Он не часть трека, это «виртуальная» лямбда группы. В 99% случаев `nullptr`.

### 3.7. SsTrack (0x18 байт) / SsLambda (0x10 байт)

**SsTrack:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 8 | `m_trackId` | SID64 имени трека. |
| 0x08 | 2 | `m_trackIdx` | Индекс трека внутри группы. |
| 0x0A | 2 | `m_totalLambdaCount` | Число элементов в `m_pSsLambda`. |
| 0x0C | 4 | `m_padding` | Reserved. |
| 0x10 | 8 | `m_pSsLambda` | Указатель на `SsLambda[m_totalLambdaCount]`. **Relocated**. |

**SsLambda:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 8 | `m_pScriptLambda` | Указатель на `ScriptLambda`. **Relocated**. Может быть `nullptr`. |
| 0x08 | 8 | `m_someSortOfCounter` | Монотонно растущий счётчик; может иметь пропуски. Семантика неясна. |

**Ловушка:** `SsLambda` — это **обёртка** над `ScriptLambda` с метаданными. Реальный исполняемый код — в `m_pScriptLambda`. Если `m_pScriptLambda == nullptr` — слот «пустой» (может быть от удалённого кода).

---

## 4. Relocation model

### 4.1. What gets relocated

Reloc-бит стоит только на 8-байтовых слотах, которые содержат указатели. В DC-формате это:

- `DCEntry.m_entryPtr`.
- `ScriptLambda.m_pInstruction`, `m_pSymbols`.
- `StateScript.m_pSsDeclList`, `m_pSsOptions`, `m_pSsStateTable`, `m_pDebugFileName`, `m_pErrorName`.
- `SsDeclarationList.m_pDeclarations`.
- `SsDeclaration.m_declIdString`, `m_pDeclValue`.
- `SsOptions.m_optionString`, `m_pSymbolArray`, `m_symbolArray2`, `m_symbolArray3`, `m_symbolArray4`.
- `SymbolArray.m_pSymbols`.
- `SsState.m_pSsOnBlocks`.
- `SsOnBlock.m_pScriptLambda`.
- `SsTrackGroup.m_aTracks`, `m_name`, `m_rareScriptLambda`.
- `SsTrack.m_pSsLambda`.
- `SsLambda.m_pScriptLambda`.

**Не relocated:**
- SID64-значения (`m_nameID`, `m_typeId`, `m_stateScriptId`, `m_declId`, `m_trackId`).
- Целые/вещественные значения.
- Размеры и счётчики.
- Строковые **содержимые** (сами байты строк не relocated, relocated только указатели на них).

### 4.2. Bitmap encoding

Бит `N` в bitmap (LSB-first) соответствует 8-байтовому слоту с индексом `N`:

```
slot_index = N
file_offset = slot_index * 8
```

Соответствие в битах: `bitmap[N / 8] & (1 << (N % 8))`.

**Ловушка:** бит 0 в первом байте соответствует **первому** 8-байтовому слоту файла (оффсет `0x00`), а не последнему. То есть младший бит — старший адрес.

### 4.3. m_pointedAtTable vs m_relocTable

- `m_relocTable` — bitmap **из файла**: где **есть** указатели.
- `m_pointedAtTable` — bitmap, **построенная** при загрузке: где **лежат цели** указателей (то есть какие 8-байтовые слоты являются **адресами**, а не значениями).

`m_pointedAtTable` используется, чтобы при дампе **не** интерпретировать значение как указатель, если на него никто не ссылается. Это критично для различения структур и указателей в heap-подобных данных.

### 4.4. Re-mapping on save

При сохранении (`BinaryFile::get_unmapped`) выполняется обратное преобразование: абсолютный указатель → файловый оффсет.

```
for slot in [0, table_size * 8):
    if bitmap[slot / 8] & (1 << (slot % 8)):
        value = *(p64*)(bytes + slot * 8)
        value -= base_address
        *(p64*)(bytes + slot * 8) = value
```

**Ловушка:** после `get_unmapped` указатели становятся **оффсетами**, и файл можно записывать на диск. Но `m_pointedAtTable` при этом **не обновляется** — она относится к mapped-версии. Если после `get_unmapped` снова читать через API `BinaryFile` — получишь мусор.

**Ловушка:** при `get_unmapped` из `value` вычитается текущий `m_bytes.get()`. Если файл был загружен по одному адресу, а save происходит — по другому (например, в другом процессе), это не сработает. `get_unmapped` **не является** идемпотентным.

---

## 5. Points of caution

### 5.1. «Pointer but no count» — массивы, размер которых неизвестен

**Проблема:** в некоторых структурах есть указатель на массив, но **нет** поля `count` рядом. Размер массива известен только по контексту.

Примеры:

- `SsTrackGroup.m_aTracks` — размер **есть** (`m_numTracks`).
- `SsOptions.m_pSymbolArray` → `SymbolArray.m_pSymbols` — размер **есть** (`m_numEntries`).
- `StateScript.m_pSsStateTable` — размер **есть** (`m_stateCount`).
- `StateScript.m_pDebugFileName`, `m_pErrorName` — это **строки**, не массивы; длина — до нуль-терминатора.

**Общее правило:** в DC-формате **почти всегда** есть явный `count`. Если его нет — это либо строка (до `\0`), либо **одиночный** объект (не массив).

**Исключение:** `SsOnBlock.m_trackGroup` — **встроенный** `SsTrackGroup`, а не указатель. Размер известен по типу (`0x38`).

### 5.2. Embedded pointers to functions vs data

**Проблема:** указатель может указывать на **функцию** (`ScriptLambda`) или на **данные** (строку, массив, другой объект). Различить их можно только по:

- Контексту (поле в структуре, где ожидается функция).
- Проверке «вызывается ли» указатель через `Call`/`CallFf` (в `dconstruct` есть `pointer_gets_called`).

**Ловушка:** `LookupPointer` в VM резолвит SID через внешний lookup. Это может вернуть либо `ScriptLambda*`, либо `data*`. Различие — по следующей инструкции: если за `LookupPointer` идёт `Call`/`CallFf`, это функция. Если просто чтение/запись — данные.

### 5.3. SsOptions quirks

**Проблема:** `SsOptions` содержит **4** указателя на `SymbolArray`. В норме заполнен только `m_pSymbolArray`. Остальные — `nullptr`. Но иногда (редко) заполнены и они, причём **разными** данными.

**Ловушка:** нельзя предполагать, что `m_symbolArray2/3/4` пусты. Нужно проверять каждый на `nullptr` и обрабатывать независимо. **Не** считать их массивом из 4 элементов.

**Ловушка:** `m_optionString` — это строка, но по семантике это **не** SID. Это debug-представление options. В `.dc`-файле её может не быть (`nullptr`).

### 5.4. Track groups with lambda-count vs track-count

**Проблема:** в `SsTrackGroup` есть **два** разных счётчика:

- `m_numTracks` — число **треков** (`SsTrack`).
- `m_totalLambdaCount` — суммарное число **лямбд** во всех треках.

Это **разные** сущности. `m_totalLambdaCount` **не равно** `m_numTracks` и **не равно** `m_numTracks * N`.

**Ловушка:** при парсинге `SsTrackGroup` **нельзя** использовать `m_totalLambdaCount` для определения размера `m_aTracks`. Для этого — только `m_numTracks`. А `m_totalLambdaCount` — для валидации (сумма `SsTrack[i].m_totalLambdaCount` должна совпадать).

### 5.5. ScriptLambda.m_sum formula discrepancy

**Проблема:** `m_sum` документирован как «общий размер блока в байтах», но формула в `get_scriptlambda_sum()`:

```cpp
return 12 + 4 * (m_instructions.size() + m_symbolTable.size());
```

Это **не** `sizeof(ScriptLambda) + ...`. Это `12 + 4*(numInstr + numSym)`. Неясно, что именно означают 12 и 4. Возможно, это размер в **32-битных единицах** для чего-то (например, для паковки в 32-битный контейнер).

**Ловушка:** не использовать `m_sum` для вычисления размера инструкций / символов в байтах. Только для чтения полей — использовать `m_numInstructions` и `m_numSymbols`.

### 5.6. SID resolution timing

**Проблема:** SID64 в DC-файле — это **число**, а не строка. Чтобы получить строку, нужно:

1. Загрузить `sidbase.bin` — глобальный реестр SID → string.
2. Для каждого SID64 в файле — найти в реестре.

**Ловушка:** если `sidbase.bin` не загружен, все SID будут отображаться как `0x...`. Это не ошибка файла, это отсутствие контекста.

**Ловушка:** в `m_sidCache` (в `BinaryFile`) сохраняются **все** SID, встречающиеся в файле, с резолвом из `sidbase`. Если `sidbase` потом заменить — `m_sidCache` **не обновится**.

### 5.7. String table interning

**Проблема:** при кодогенерации одинаковые строки интернируются — в string table попадает **одна** копия. Все указатели на неё — на один оффсет.

**Ловушка:** при декомпиляции нельзя предполагать, что разные указатели указывают на разные строки. Два `m_pDebugFileName` из разных `StateScript` могут указывать на **один** оффсет, если строка одинаковая.

**Ловушка:** `BinaryFile::replace_newlines_in_stringtable` **модифицирует** string table (заменяет `\n` на пробел). Это делается один раз при загрузке. Если после этого сохранить файл (`get_unmapped`), string table уже изменена — это **не** round-trip-совместимо с оригиналом.

### 5.8. Relocated vs raw values at the same offset

**Проблема:** один и тот же 8-байтовый слот может быть:

- **Указателем** (reloc-бит установлен) — тогда значение в файле = оффсет, после загрузки = адрес.
- **Сырым значением** (reloc-бит не установлен) — значение остаётся как есть.

**Ловушка:** при парсинге структуры **нельзя** определить по значению, указатель это или нет. Только по reloc-биту. Поэтому `BinaryFile` всегда сначала применяет reloc-таблицу, а потом уже всё остальное.

**Ловушка:** при `get_unmapped` (сохранении) **нельзя** просто так поменять один из указателей на сырое значение — это сломает позицию reloc-бита. Reloc-бит **статичен** и не зависит от значения.

---

## 6. Compiler ↔ VM boundary

### 6.1. How a ScriptLambda ends up in the file

Типичный pipeline (в `dconstruct`/`carbon`):

```
DCPL source
    │
    ▼
Lexer → tokens
    │
    ▼
Parser → ast::program
    │
    ▼
Semantic checks (scope, types)
    │
    ▼
ast::function_definition::emit_dc
    │   ├── compilation::function (registers, instructions)
    │   └── returns program_binary_element
    ▼
ast::program::make_binary
    │   ├── собирает все элементы в один буфер
    │   ├── строит header
    │   ├── строит entries
    │   ├── строит string table
    │   └── строит reloc-таблицу
    ▼
.dc binary file
```

Ключевой момент: `program_binary_element` — это **промежуточное представление**, где данные уже сериализованы (bytes) и параллельно заполнена reloc-таблица (bitmap). На этапе `make_binary` всё это **объединяется** в один файл.

### 6.2. How the VM reads it back

```
.dc binary file
    │
    ▼
BinaryFile::from_path
    │   ├── читает файл в буфер
    │   ├── проверяет magic/version
    │   └── read_reloc_table (применяет reloc, строит m_pointedAtTable)
    ▼
BinaryFile (mapped, указатели абсолютные)
    │
    ▼
Disassembler / BinaryFileInspector
    │   ├── читает header, entries
    │   ├── для каждой entry — парсит ScriptLambda / StateScript
    │   └── строит m_sidCache
    ▼
function_disassembly / state_script tree
    │
    ▼
dcompiler::decomp_function → ast::function_definition
```

### 6.3. What is currently NOT round-trippable

Известные проблемы на границе compiler ↔ VM:

1. **StateScript в целом.** Компилятор умеет собирать **одну** лямбду. Для полного state-script (`SsOptions`, `SsDeclarationList`, `SsState`, `SsOnBlock`, `SsTrackGroup`, `SsTrack`, `SsLambda`) — пайплайн не замкнут.
2. **Строки.** При кодогенерации строки интернируются в string table. При декомпиляции строки превращаются в `std::string` в AST. При обратной компиляции — снова интернируются, но **порядок** может отличаться → другой оффсет в файле → не побайтовое совпадение.
3. **Пустые лямбды.** `SsLambda` с `m_pScriptLambda == nullptr` — слот «пустой». Компилятор может его не восстановить.
4. **m_sum.** Формула не совпадает с «суммой байтов», поэтому при пересборке значение может отличаться от оригинала.
5. **Порядок в string table.** Оригинал может иметь строки в другом порядке, чем генерирует компилятор. Это допустимо (интернирование), но ломает побайтовое сравнение.
6. **Padding/выравнивание.** Может отличаться между компилятором и оригиналом (например, `m_padding` заполнено нулями vs мусором).

### 6.4. The missing pointer problem (open)

**Открытая проблема:** при передаче лямбды в VM **не хватает какого-то указателя**, из-за которого VM не может найти код или константы.

Гипотезы:

1. **`m_sidGlobal`.** VM ожидает, что это `SID("global")` для top-level лямбд. Если положить другое значение — VM не найдёт scope.
2. **`m_pInstruction` vs `m_pSymbols`.** VM ожидает, что symbol table **следует сразу за** инструкциями (по оффсету `m_pInstruction + sizeof(Instruction) * m_numInstructions`). Если `m_pSymbols` указывает в другое место — VM не найдёт символы.
3. **Порядок полей в `ScriptLambda`.** Если твой формат имеет другое расположение полей, чем ожидает VM, — все указатели читаются из неправильных мест.
4. **Отсутствие `m_numSymbols` в каноне.** Если VM использует неявный размер symbol table (по формуле `m_sum`), а не поле `m_numSymbols`, — это объясняет, почему поле «неканонично».

**Что нужно, чтобы закрыть:** трассировка VM при загрузке `ScriptLambda` — какие поля она читает и в каком порядке. Это требует дизассемблера самой VM (`parse_instruction.cpp` в исходниках — часть ответа).

---

## 7. Checks and invariants

### 7.1. Structural invariants

Проверять после парсинга:

1. `sizeof(DC_Header) == 0x20`.
2. `sizeof(DCEntry) == 0x18`.
3. `sizeof(ScriptLambda) == 0x58` (T2R) или `0x50` (UC4).
4. `sizeof(StateScript) == 0x50`.
5. `sizeof(SsDeclarationList) == 0x10`.
6. `sizeof(SsDeclaration) == 0x30`.
7. `sizeof(SymbolArray) == 0x10`.
8. `sizeof(SsOptions) == 0x50`.
9. `sizeof(SsState) == 0x18`.
10. `sizeof(SsOnBlock) == 0x50`.
11. `sizeof(SsTrackGroup) == 0x38`.
12. `sizeof(SsTrack) == 0x18`.
13. `sizeof(SsLambda) == 0x10`.

### 7.2. Relocation invariants

1. `m_textSize >= 0x20` и `m_textSize <= m_size`.
2. `m_stringsOffset >= 0x20 + 0x18 * m_numEntries` (то есть >= конец entries).
3. `m_stringsOffset < m_textSize`.
4. `table_size == (u32*)(bytes + m_textSize)` — первые 4 байта reloc-таблицы.
5. `table_size == ceil(m_textSize / 64)`.
6. `m_textSize + 4 + table_size <= m_size`.
7. После `read_reloc_table` каждый указатель лежит в `[m_bytes, m_bytes + m_size)`.

### 7.3. Suggested runtime asserts

Для отладки (`#ifndef NDEBUG`):

```cpp
// После загрузки файла:
assert(m_dcheader->m_magic == DC_FILE_MAGIC);
assert(m_dcheader->m_versionNumber == DC_FILE_VERSION);
assert(m_dcheader->m_textSize >= sizeof(DC_Header));
assert(m_dcheader->m_textSize <= m_size);
assert(m_dcheader->m_stringsOffset < m_dcheader->m_textSize);
assert(m_dcheader->m_numEntries < 100000);  // sanity

// После read_reloc_table:
assert(m_relocTable.m_ptr == m_bytes.get() + m_dcheader->m_textSize + 4);

// При чтении entry:
for (auto& entry : entries) {
    assert(entry.m_nameID != 0 || entry.m_typeId == 0);  // либо оба нули, либо name задан
    assert(entry.m_typeId == SID("script-lambda") || entry.m_typeId == SID("state-script"));
}
```

---

## 8. Appendices

### A. Constant SIDs used by the format

| SID | Значение | Где используется |
|---|---|---|
| `SID("array")` | маркер массива | перед `DCEntry[]`, `SsState[]`, ... |
| `SID("script-lambda")` | тип entry | `DCEntry.m_typeId`, `ScriptLambda.m_typeId` |
| `SID("state-script")` | тип entry | `DCEntry.m_typeId` |
| `SID("global")` | scope | `ScriptLambda.m_sidGlobal` |
| `SID("boolean")` | тип переменной | `SsDeclaration.m_declTypeId` |
| `SID("int32")` | тип переменной | `SsDeclaration.m_declTypeId` |
| `SID("float")` | тип переменной | `SsDeclaration.m_declTypeId` |
| `SID("string")` | тип переменной | `SsDeclaration.m_declTypeId` |
| `SID("symbol")` | тип переменной | `SsDeclaration.m_declTypeId` |
| `SID("vector")` | тип переменной | `SsDeclaration.m_declTypeId` |
| `SID("quat")` | тип переменной | `SsDeclaration.m_declTypeId` |
| `SID("point")` | тип переменной | `SsDeclaration.m_declTypeId` |
| `SID("timer")` | тип переменной | `SsDeclaration.m_declTypeId` |
| `SID("bound-frame")` | тип переменной | `SsDeclaration.m_declTypeId` |

### B. Byte-size cheat sheet

| Тип | Размер |
|---|---|
| `sid64`, `u64`, `i64`, `f64`, `p64` | 8 |
| `sid32`, `u32`, `i32`, `f32` | 4 |
| `u16`, `i16` | 2 |
| `u8`, `i8`, `bool` | 1 |
| `Instruction` (T2R) | 8 |
| `ShortInstruction` (UC4) | 4 |
| Указатель | 8 |

### C. Sample hex walkthrough

Пусть есть файл `test.bin` размером `0x100` со следующим layout (упрощённо):

```
0x00  DC_Header:
      44 43 30 30    magic
      01 00 00 00    version
      90 00 00 00    m_textSize = 0x90
      58 00 00 00    m_stringsOffset = 0x58
      01 00 00 00    m_always1
      01 00 00 00    m_numEntries = 1
      28 00 00 00 00 00 00 00    m_pStartOfData = 0x28

0x20  61 72 72 61 79 00 00 00    SID("array") — маркер

0x28  DCEntry[0]:
      [8]  nameID   = SID("#func")
      [8]  typeId   = SID("script-lambda")
      [8]  entryPtr = 0x40 (relocated)

0x40  ScriptLambda:
      [8]  m_type             = SID("script-lambda")
      [8]  m_pInstruction     = 0x98 (relocated, указывает ВНЕ m_textSize? — ошибка!)
      ...
```

**В этом примере — ошибка:** `m_pInstruction` указывает за пределы `m_textSize`. При парсинге `BinaryFile` упадёт на assert. Это иллюстрация важности инварианта #4 из секции 7.2.

В реальном файле `m_pInstruction` всегда в `[0x00, m_textSize)`.

---

## Заключение

DC-формат — это **relocatable контейнер** с плоской иерархией:

```
DC_Header
  └─ DCEntry[N]
       └─ ScriptLambda | StateScript
                        └─ SsDeclarationList | SsOptions | SsState[]
                                                          └─ SsOnBlock[]
                                                               └─ SsTrackGroup
                                                                    └─ SsTrack[]
                                                                         └─ SsLambda[]
                                                                              └─ ScriptLambda
```

Каждый уровень содержит явный `count` (кроме строк и одиночных полей). Указатели relocated через bitmap по `m_textSize`. Релокация — обратимая.

**Главные ловушки:**
1. `m_numSymbols` в `ScriptLambda` — notcanonical, но используется.
2. `SsTrackGroup.m_totalLambdaCount` ≠ число треков.
3. `SsOptions` — 4 разных указателя, не массив.
4. `m_sum` — не байты.
5. `m_pStartOfData` — таблица entries, не «данные».
6. Reloc-бит статичен и не зависит от значения.

**Открытая проблема:** при передаче лямбды в VM не хватает указателя — надо трассировать VM.

---

*Конец файла.*