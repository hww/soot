#include <gtest/gtest.h>
#include "common/sooti/Interpreter.hpp"

using namespace script;

class HashTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        env = std::make_shared<EnvironmentObject>();
    }

    Object eval(const std::string& code) {
        Object obj = interp.get_reader().read_from_string(code, "test");
        return interp.eval_form(obj, env);
    }

    Interpreter interp;
    std::shared_ptr<EnvironmentObject> env;
};

TEST_F(HashTableTest, HashTableCreation) {
    Object obj = eval("(make-hash-table)");
    EXPECT_TRUE(obj.is_hash_table());
}

TEST_F(HashTableTest, HashTableSetAndRef) {
    eval("(begin \
        (define ht (make-hash-table)) \
        (hash-table-set! ht \"key1\" \"value1\") \
        (hash-table-set! ht 'key2 42) \
    )");

    Object obj = eval("(hash-table-ref ht \"key1\")");
    EXPECT_TRUE(obj.is_string());
    EXPECT_EQ(obj.as_string()->data, "value1");

    obj = eval("(hash-table-ref ht 'key2)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 42);
}

// ���� hash-table?
TEST_F(HashTableTest, HashTablePredicate) {
    Object obj = eval("(hash-table? (make-hash-table))");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(hash-table? 42)");
    EXPECT_FALSE(obj.as_boolean());
}