# Устройство файла `sidbase.bin` (исправленная версия)

Файл `sidbase.bin` — это **отсортированный по хэшу массив записей**, каждая из которых связывает 64-битный хэш SID со смещением строки в файле. Используется для обратного поиска: по хэшу восстановить исходную строку.

---

## 1. Формат файла

```
┌─────────────────────────────────────┐
│  u64 numEntries                     │  offset 0, size 8
├─────────────────────────────────────┤
│  SIDBaseEntry[0]                    │  offset 8
│  SIDBaseEntry[1]                    │  ...
│  ...                                │
│  SIDBaseEntry[numEntries-1]         │  offset 8 + (numEntries-1)*16
├─────────────────────────────────────┤
│  Строковый пул                      │  offset 8 + numEntries*16
│  "str1\0str2\0str3\0..."            │
└─────────────────────────────────────┘
```

### Структура записи

```cpp
struct SIDBaseEntry {
    sid64 hash;   // 8 байт — хэш строки (FNV-1a 64)
    u64   offset; // 8 байт — смещение строки ОТ НАЧАЛА ФАЙЛА
};
```

Итого **16 байт** на запись.

### Ключевые факты

1. **Записи отсортированы по `hash` по возрастанию.** Это критично для бинарного поиска.
2. **`offset` — от начала файла, а не от начала строкового пула.** Это видно из `search`:
   ```cpp
   return reinterpret_cast<const char*>(m_sidbytes.get() + current->offset);
   ```
   где `m_sidbytes` — это указатель на **начало файла** (не на пул).
3. **Строки нуль-терминированы.** Каждая строка заканчивается `\0`.
4. **Порядок строк в пуле не связан с порядком entries.** Строки могут идти в произвольном порядке — важно только, что `offset` каждой записи указывает на её строку.
5. **Заголовок — ровно 8 байт.** Никакой магии, версии или флагов.

---

## 2. Пример содержимого

Пусть 3 строки: `"joel"`, `"tommy"`, `"ellie"`. Хэши в порядке возрастания дают порядок `joel < tommy < ellie` (условно).

```
Offset      Bytes                          Meaning
──────────  ─────────────────────────────  ──────────────────────────
0x00000000  03 00 00 00 00 00 00 00        numEntries = 3
0x00000008  <hash("joel")>                 SIDBaseEntry[0].hash
0x00000010  38 00 00 00 00 00 00 00        SIDBaseEntry[0].offset = 0x38
0x00000018  <hash("tommy")>                SIDBaseEntry[1].hash
0x00000020  3D 00 00 00 00 00 00 00        SIDBaseEntry[1].offset = 0x3D
0x00000028  <hash("ellie")>                SIDBaseEntry[2].hash
0x00000030  43 00 00 00 00 00 00 00        SIDBaseEntry[2].offset = 0x43
0x00000038  "joel\0"                       (5 байт)
0x0000003D  "tommy\0"                      (6 байт)
0x00000043  "ellie\0"                      (6 байт)
```

Проверка: `m_sidbytes.get() + 0x38` = `temp_buffer + 0x38` = адрес строки `"joel"`. Верно.

Строки в пуле идут в порядке, в котором они были в исходном файле (или в любом другом — это не влияет на корректность).

---

## 3. Поиск по хэшу

```cpp
const char* SIDBase::search(sid64 hash) const noexcept {
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
    return nullptr;
}
```

Классический бинарный поиск `O(log N)`. Если хэш не найден — `nullptr`.

---

## 4. Как этим пользуется `Disassembler`

```cpp
const char* Disassembler::lookup(sid64 sid) {
    auto res = m_currentFile->m_sidCache.find(sid);
    if (res != m_currentFile->m_sidCache.end())
        return res->second.c_str();

    const char* hash_string = m_sidbase->search(sid);
    if (hash_string == nullptr) {
        auto [iter, _] = m_currentFile->m_sidCache.emplace(
            sid, int_to_string_id<sid64>(sid));
        hash_string = iter->second.c_str();
    } else {
        m_currentFile->m_sidCache.emplace(sid, hash_string);
    }
    return hash_string;
}
```

`search()` возвращает указатель в `m_sidbytes`, кэш хранит `std::string` (копию), чтобы не зависеть от времени жизни `SIDBase`.

---

## 5. Ограничения и предположения

| Предположение | Где используется |
|---|---|
| Записи отсортированы по `hash` | `SIDBase::search` (бинарный поиск) |
| `offset` — от начала файла | `SIDBase::search` (`m_sidbytes.get() + offset`) |
| Строки нуль-терминированы | `reinterpret_cast<const char*>` + `%s` |
| `m_entries[0].hash` = минимум | `Disassembler::is_unmapped_sid` (быстрый фильтр) |
| `m_entries[N-1].hash` = максимум | то же |
| Файл читается целиком в память | `new std::byte[fsize]` |

---

## 6. Краткая схема

```
sidbase.bin
├── u64 numEntries
├── SIDBaseEntry[numEntries]   (16 байт каждый, отсортированы по hash)
│   ├── sid64 hash
│   └── u64   offset  ← от начала ФАЙЛА
└── string pool               ("str1\0str2\0...")
```

---

