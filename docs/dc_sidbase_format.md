# Устройство файла sidbase.bin

Судя по коду (в частности `source/base/sidbase.cpp` и `source/disassembly/disassembler.cpp`), файл `sidbase.bin` — это **отсортированный по хэшу массив строковых идентификаторов (SID)**. Он используется для «обратного» поиска: по 64-битному (или 32-битному) хэшу восстановить исходную строку.

---

## 1. Формат файла

Файл имеет очень простую бинарную структуру:

```
┌─────────────────────────────┐
│  u64 num_entries            │  ← количество записей
├─────────────────────────────┤
│  SIDBaseEntry[0]            │
│  SIDBaseEntry[1]            │
│  ...                        │
│  SIDBaseEntry[num_entries-1]│
├─────────────────────────────┤
│  Строковый пул (raw bytes)  │  ← все строки подряд, без разделителей
└─────────────────────────────┘
```

### Структура одной записи

Из кода видно, что `SIDBaseEntry` — это пара `(hash, offset)`:

```cpp
// Реконструкция из sidbase.cpp:
// entries = reinterpret_cast<SIDBaseEntry*>(temp_buffer + 8);
// current->hash
// current->offset
// m_sidbytes.get() + current->offset  → указатель на строку

struct SIDBaseEntry {
    sid64 hash;   // 8 байт — хэш строки (например, FNV-1a 64)
    u64   offset; // 8 байт — смещение в строковом пуле (от начала пула)
};
```

Итого: **16 байт на запись**.

### Полная C++-модель файла

```cpp
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <expected>
#include <fstream>
#include <filesystem>

namespace dconstruct {

using u64   = uint64_t;
using sid64 = uint64_t;
using sid32 = uint32_t;

// Одна запись в таблице поиска
#pragma pack(push, 1)
struct SIDBaseEntry {
    sid64 hash;   // хэш SID (обычно 64-битный FNV-1a)
    u64   offset; // смещение строки в строковом пуле
};
#pragma pack(pop)

// Заголовок файла (всего 8 байт)
#pragma pack(push, 1)
struct SIDBaseHeader {
    u64 numEntries; // количество записей в таблице
};
#pragma pack(pop)

// Рантайм-представление загруженного файла
class SIDBase {
public:
    static std::expected<SIDBase, std::string>
    from_binary(const std::filesystem::path& path) noexcept;

    // Бинарный поиск по отсортированному массиву
    [[nodiscard]] const char* search(sid64 hash) const noexcept;
    [[nodiscard]] bool        sid_exists(sid64 hash) const noexcept;

    // Публичные поля, которые использует Disassembler:
    u64              m_numEntries = 0;
    std::unique_ptr<std::byte[]> m_sidbytes; // весь файл в памяти
    SIDBaseEntry*    m_entries   = nullptr;  // = m_sidbytes + 8
    sid64            m_lowestSid  = 0;       // m_entries[0].hash
    sid64            m_highestSid = 0;       // m_entries[numEntries-1].hash
};

} // namespace dconstruct
```

---

## 2. Как это загружается (по шагам из `sidbase.cpp`)

```cpp
std::expected<SIDBase, std::string>
SIDBase::from_binary(const std::filesystem::path& path) noexcept
{
    std::ifstream sidfile(path, std::ios::binary);
    if (!sidfile.is_open())
        return std::unexpected{"couldn't open sidbase at path '" + path.string() + "'\n"};

    const std::size_t fsize = std::filesystem::file_size(path);
    std::byte* temp_buffer = new std::byte[fsize];

    // 1. Читаем количество записей (первые 8 байт)
    u64 num_entries = 0;
    sidfile.read(reinterpret_cast<char*>(&num_entries), 8);
    sidfile.seekg(0);

    // 2. Читаем весь файл целиком
    sidfile.read(reinterpret_cast<char*>(temp_buffer), fsize);

    // 3. Записи начинаются сразу после заголовка
    auto entries = reinterpret_cast<SIDBaseEntry*>(temp_buffer + 8);
    auto bytes   = std::unique_ptr<std::byte[]>(temp_buffer);

    // 4. Запоминаем границы для быстрой проверки «в диапазоне ли хэш»
    const sid64 lowest  = entries[0].hash;
    const sid64 highest = entries[num_entries - 1].hash;

    return SIDBase{num_entries, std::move(bytes), entries, lowest, highest};
}
```

**Важные моменты:**
- Записи **отсортированы по `hash`** по возрастанию — это критично для бинарного поиска.
- Строки лежат в конце файла единым пулом; `offset` каждой записи указывает **от начала пула** (не от начала файла).
- Файл читается в память целиком, чтобы не делать `seek` при каждом поиске.

---

## 3. Поиск по хэшу

```cpp
const char* SIDBase::search(sid64 hash) const noexcept
{
    u64 low = 0, high = m_numEntries - 1, mid = 0;

    while (low <= high) {
        mid = low + (high - low) / 2;
        const SIDBaseEntry* cur = m_entries + mid;

        if (cur->hash == hash) [[unlikely]]
            return reinterpret_cast<const char*>(m_sidbytes.get() + cur->offset);

        if (cur->hash < hash)
            low = mid + 1;
        else {
            if (mid == 0) break;
            high = mid - 1;
        }
    }
    return nullptr; // не найдено
}
```

Классический бинарный поиск `O(log N)`. Если хэш не найден — `nullptr`.

---

## 4. Пример содержимого файла на диске

Пусть, например, в sidbase 3 строки: `"ellie"`, `"joel"`, `"tommy"`. Тогда файл может выглядеть так:

```
Offset      Bytes                                          Meaning
──────────  ─────────────────────────────────────────────  ─────────────────────────
0x00000000  03 00 00 00 00 00 00 00                        num_entries = 3
0x00000008  <hash("ellie")  8 байт>                        SIDBaseEntry[0].hash
0x00000010  <offset 0x0000000000000028>                    SIDBaseEntry[0].offset
0x00000018  <hash("joel")   8 байт>                        SIDBaseEntry[1].hash
0x00000020  <offset 0x000000000000002E>                    SIDBaseEntry[1].offset
0x00000028  <hash("tommy")  8 байт>                        SIDBaseEntry[2].hash
0x00000030  <offset 0x0000000000000033>                    SIDBaseEntry[2].offset
0x00000038  "ellie\0joel\0tommy\0"                         строковый пул (6+5+6 = 17 байт)
```

Обрати внимание: `entries[0].offset = 0x28` — это смещение **от начала пула**, а сам пул начинается с `8 + 3*16 = 0x38`. То есть `m_sidbytes.get() + 0x28` = адрес строки `"ellie"`.

---

## 5. Как этим пользуется `Disassembler`

В `disassembler.cpp`:

```cpp
const char* Disassembler::lookup(sid64 sid) {
    auto res = m_currentFile->m_sidCache.find(sid);
    if (res != m_currentFile->m_sidCache.end())
        return res->second.c_str();          // кэш попаданий

    const char* hash_string = m_sidbase->search(sid);
    if (hash_string == nullptr) {
        // строки нет в базе — генерируем "0xDEADBEEF..."
        auto [iter, _] = m_currentFile->m_sidCache.emplace(
            sid, int_to_string_id<sid64>(sid));
        hash_string = iter->second.c_str();
    } else {
        m_currentFile->m_sidCache.emplace(sid, hash_string); // кэшируем
    }
    return hash_string;
}
```

Так как `search()` возвращает указатель на память внутри `m_sidbytes`, кэш хранит `std::string` (копию), чтобы не зависеть от времени жизни `SIDBase`.

---

## 6. Ограничения и предположения

| Предположение | Где используется |
|---|---|
| Записи отсортированы по `hash` | `SIDBase::search` (бинарный поиск) |
| `m_entries[0].hash` = минимальный, `m_entries[N-1].hash` = максимальный | `Disassembler::is_unmapped_sid` |
| Все строки лежат в конце файла одним пулом | `m_sidbytes.get() + entry.offset` |
| Строки нуль-терминированы | `reinterpret_cast<const char*>` + `%s` в printf |
| Файл читается целиком в память | `new std::byte[fsize]` |

`is_unmapped_sid` проверяет, что значение лежит в `[m_lowestSid, m_highestSid]` — это быстрый предварительный фильтр, чтобы не дёргать бинарный поиск для явно мусорных чисел.

---

## 7. Итоговая краткая схема

```
sidbase.bin
├── u64 numEntries
├── SIDBaseEntry[numEntries]   (16 байт каждый, отсортированы по hash)
│   ├── sid64 hash
│   └── u64   offset → в пул строк
└── string pool (raw bytes, "str1\0str2\0...")

Поиск: binary search по hash → offset → указатель на строку в пуле.
```