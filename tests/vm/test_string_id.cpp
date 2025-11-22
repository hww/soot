#include "gtest/gtest.h"
#include "vm/export.hpp"

using namespace vm;

TEST(StringId, BasicOperations) {
    vm::StringId sid1 = vm::string_id::from_string("hello");
    vm::StringId sid2 = vm::string_id::from_string("hello");
    vm::StringId sid3 = vm::string_id::from_string("world");

    EXPECT_EQ(sid1, sid2);
    EXPECT_NE(sid1, sid3);
    EXPECT_NE(sid1, 0u);
}

TEST(StringId, LiteralOperator) {
    auto sid1 = "test"_sid;
    auto sid2 = "test"_sid;

    EXPECT_EQ(sid1, sid2);
    EXPECT_EQ(vm::string_id::to_string(sid1), "test");
}

TEST(StringId, StringLookup) {
    auto sid = "debug_string"_sid;
    EXPECT_EQ(vm::string_id::to_string(sid), "debug_string");

    vm::StringId unknown = 0x12345678;
    EXPECT_TRUE(vm::string_id::to_string(unknown).find("12345678") != std::string::npos);
}

TEST(StringId, STLCompatibility) {
    std::unordered_map<vm::StringId, int> map;

    map["key1"_sid] = 100;
    map["key2"_sid] = 200;

    EXPECT_EQ(map["key1"_sid], 100);
    EXPECT_EQ(map["key2"_sid], 200);
}