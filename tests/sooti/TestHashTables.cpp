#include "common/sooti/Interpreter.hpp"
#include <gtest/gtest.h>

using namespace script;

class HashTableTest : public ::testing::Test {
  protected:
    void SetUp() override {
        env = interp.get_global_environment().as_env();
    }

    Object eval(const std::string &code) {
        return interp.eval_string(code, "test");
    }

    Interpreter        interp;
    EnvironmentObject *env;
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

// hash-table?
TEST_F(HashTableTest, HashTablePredicate) {
    Object obj = eval("(type? 'hash-table (make-hash-table))");
    EXPECT_TRUE(obj.as_boolean());
}