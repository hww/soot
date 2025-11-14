#include <gtest/gtest.h>
#include "interpreter.h"

class HashTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        env = std::make_shared<EnvironmentObject>();
    }

    Object eval(const std::string& code) {
        Object obj = interp.get_reader().read_from_string(code, "test");
        return interp.eval_with_rewind(obj, env);
    }

    Interpreter interp;
    std::shared_ptr<EnvironmentObject> env;
};

// Тест создания хэш-таблиц
TEST_F(HashTableTest, HashTableCreation) {
    Object obj = eval("(make-hash-table)");
    EXPECT_TRUE(obj.is_hash_table());
}

// Тест hash-table-set! и hash-table-ref
TEST_F(HashTableTest, HashTableSetAndRef) {
    // Все выражения выполняются в одном eval с begin
    eval("(begin \
        (define ht (make-hash-table)) \
        (hash-table-set! ht \"key1\" \"value1\") \
        (hash-table-set! ht 'key2 42) \
    )");

    // Теперь ht определен в окружении env
    Object obj = eval("(hash-table-ref ht \"key1\")");
    EXPECT_TRUE(obj.is_string());
    EXPECT_EQ(obj.as_string(), "value1");

    obj = eval("(hash-table-ref ht 'key2)");
    EXPECT_TRUE(obj.is_integer());
    EXPECT_EQ(obj.as_integer(), 42);
}

// Тест hash-table?
TEST_F(HashTableTest, HashTablePredicate) {
    Object obj = eval("(hash-table? (make-hash-table))");
    EXPECT_TRUE(obj.as_boolean());

    obj = eval("(hash-table? 42)");
    EXPECT_FALSE(obj.as_boolean());
}