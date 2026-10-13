#include <gtest/gtest.h>

#include "carbon/file/DCScript.hpp"
#include "carbon/gc/GC.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/vm/NativeFunc.hpp"
#include "carbon/vm/NativeMap.hpp"

namespace carbon::testing {

    namespace {

        Variant make_key(const char *name) { return Variant(static_cast<i64>(SID(name))); }

    } // namespace

    TEST(MapRuntime, MakeAndCount) {
        gc::initialize(16, 256);
        NativeMap::initialize_builtins();

        Variant m = native_make_map(0, nullptr);
        ASSERT_TRUE(m.is_ptr());
        ASSERT_NE(m.get_ptr(), nullptr);

        Variant count = native_map_count(1, &m);
        EXPECT_EQ(count.get_i64(), 0);

        gc::shutdown();
    }

    TEST(MapRuntime, SetGet) {
        gc::initialize(16, 256);
        NativeMap::initialize_builtins();

        Variant m = native_make_map(0, nullptr);

        // (map-set m #health 100)
        Variant set_args[3] = {m, make_key("health"), Variant(static_cast<i64>(100))};
        native_map_set(3, set_args);

        // (map-get m #health)
        Variant get_args[2] = {m, make_key("health")};
        Variant result = native_map_get(2, get_args);
        EXPECT_EQ(result.get_i64(), 100);

        // (map-count m)
        Variant count = native_map_count(1, &m);
        EXPECT_EQ(count.get_i64(), 1);

        gc::shutdown();
    }

    TEST(MapRuntime, SetOverwrite) {
        gc::initialize(16, 256);
        NativeMap::initialize_builtins();

        Variant m = native_make_map(0, nullptr);
        Variant key = make_key("health");

        Variant args1[3] = {m, key, Variant(static_cast<i64>(100))};
        native_map_set(3, args1);

        Variant args2[3] = {m, key, Variant(static_cast<i64>(200))};
        native_map_set(3, args2);

        Variant get_args[2] = {m, key};
        Variant result = native_map_get(2, get_args);
        EXPECT_EQ(result.get_i64(), 200);

        Variant count = native_map_count(1, &m);
        EXPECT_EQ(count.get_i64(), 1);

        gc::shutdown();
    }

    TEST(MapRuntime, HasAndDel) {
        gc::initialize(16, 256);
        NativeMap::initialize_builtins();

        Variant m = native_make_map(0, nullptr);
        Variant key = make_key("health");

        Variant args[3] = {m, key, Variant(static_cast<i64>(100))};
        native_map_set(3, args);

        Variant has_args[2] = {m, key};
        EXPECT_TRUE(native_map_has(2, has_args).get_bool());

        native_map_del(2, has_args);
        EXPECT_FALSE(native_map_has(2, has_args).get_bool());

        Variant count = native_map_count(1, &m);
        EXPECT_EQ(count.get_i64(), 0);

        gc::shutdown();
    }

    TEST(MapRuntime, ManyInserts) {
        gc::initialize(16, 256);
        NativeMap::initialize_builtins();

        Variant m = native_make_map(0, nullptr);

        // Insert 100 keys, verify count.
        for (i32 i = 0; i < 100; ++i) {
            const std::string name = "key_" + std::to_string(i);
            Variant           args[3] = {m, Variant(static_cast<i64>(StringId(name))),
                                         Variant(static_cast<i64>(i))};
            native_map_set(3, args);
        }

        Variant count = native_map_count(1, &m);
        EXPECT_EQ(count.get_i64(), 100);

        // Verify each.
        for (i32 i = 0; i < 100; ++i) {
            const std::string name = "key_" + std::to_string(i);
            Variant           get_args[2] = {m, Variant(static_cast<i64>(StringId(name)))};
            Variant           result = native_map_get(2, get_args);
            EXPECT_EQ(result.get_i64(), i);
        }

        gc::shutdown();
    }

} // namespace carbon::testing