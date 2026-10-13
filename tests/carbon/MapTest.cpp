#include "carbon/file/BinaryFile.hpp"
#include "carbon/file/DCScript.hpp"
#include "carbon/lib/Variant.hpp"
#include <gtest/gtest.h>

namespace carbon::testing {

    // Хелпер: создать map в памяти и проверить парсинг
    TEST(MAP, BasicLayout) {
        // Синтетический map: 3 элемента
        // Ключи: #health, #damage, #speed
        // Значения: 100 (int), 5.5 (float), 1 (int)

        // Создаём буфер
        struct TestMap {
            u32      size;
            u32      padding;
            sid64   *keys;
            Variant *values;
        };

        sid64 keys[] = {
            SID("health"),
            SID("damage"),
            SID("speed"),
        };

        // Variant — сложный тип, создаём вручную
        Variant values[3];
        values[0] = Variant(static_cast<i64>(100));
        values[1] = Variant(1.5f);
        values[2] = Variant(static_cast<i64>(1));

        TestMap map;
        map.size = 3;
        map.padding = 0;
        map.keys = keys;
        map.values = values;

        // Проверяем, что размер структуры совпадает
        EXPECT_EQ(sizeof(TestMap), 0x18);
        EXPECT_EQ(map.size, 3);

        // Проверяем ключи
        EXPECT_EQ(keys[0], SID("health"));
        EXPECT_EQ(keys[1], SID("damage"));
        EXPECT_EQ(keys[2], SID("speed"));

        // Проверяем значения
        EXPECT_EQ(values[0].get_i64(), 100);
        EXPECT_FLOAT_EQ(values[1].get_f32(), 1.5f);
        EXPECT_EQ(values[2].get_i64(), 1);
    }

    TEST(MAP, EmptyMap) {
        struct TestMap {
            u32      size;
            u32      padding;
            sid64   *keys;
            Variant *values;
        };

        TestMap map;
        map.size = 0;
        map.padding = 0;
        map.keys = nullptr;
        map.values = nullptr;

        EXPECT_EQ(map.size, 0);
        EXPECT_EQ(map.keys, nullptr);
        EXPECT_EQ(map.values, nullptr);
    }

} // namespace carbon::testing