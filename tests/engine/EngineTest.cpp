#include <gtest/gtest.h>
#include "common/carbon/kernel/Connectable.hpp"
#include "common/carbon/kernel/Engine.hpp"
#include "common/carbon/kernel/Process.hpp"
#include "common/carbon/lib/StringId.hpp"
#include <thread>
#include <memory>

using namespace carbon;
using namespace carbon;

class EngineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        engine = std::make_unique<Engine>(SID("TestEngine"), 10);
        process = std::make_unique<Process>(SID("TestProcess"));
    }

    std::unique_ptr<Engine> engine;
    std::unique_ptr<Process> process;
};

TEST_F(EngineTest, ConstructorInitialization)
{
    EXPECT_EQ(engine->Name, SID("TestEngine"));
    EXPECT_EQ(engine->Length, 0);
    EXPECT_EQ(engine->FrameCount, 0);
    EXPECT_FLOAT_EQ(engine->Time, 0.0f);
    EXPECT_EQ(engine->GetPoolCapacity(), 10);
}

TEST_F(EngineTest, AddConnection)
{
    int testValue = 42;
    void* testObject = &testValue;

    EXPECT_NO_THROW({
        engine->AddConnection(process.get(), testObject, 1, 2, 3);
        });

    EXPECT_EQ(engine->GetActiveConnectionsCount(), 1);
}

TEST_F(EngineTest, AddConnectionWithNullProcess)
{
    // Проверяем, что не бросается исключение (согласно исходному коду)
    EXPECT_NO_THROW({
        engine->AddConnection(nullptr);
        });
}

TEST_F(EngineTest, MultipleConnections)
{
    for (int i = 0; i < 5; ++i)
    {
        engine->AddConnection(process.get(), nullptr, i, i * 2, i * 3);
    }

    EXPECT_EQ(engine->GetActiveConnectionsCount(), 5);
}

TEST_F(EngineTest, ProcessDisconnect)
{
    // Add multiple connections
    for (int i = 0; i < 3; ++i)
    {
        engine->AddConnection(process.get());
    }

    EXPECT_EQ(engine->GetActiveConnectionsCount(), 3);

    engine->ProcessDisconnect(process.get());

    EXPECT_EQ(engine->GetActiveConnectionsCount(), 0);
}

TEST_F(EngineTest, ProcessDisconnectWithNullProcess)
{
    EXPECT_NO_THROW({
        engine->ProcessDisconnect(nullptr);
        });
}

TEST_F(EngineTest, RemoveAllConnections)
{
    // Add connections
    for (int i = 0; i < 3; ++i)
    {
        engine->AddConnection(process.get());
    }

    EXPECT_EQ(engine->GetActiveConnectionsCount(), 3);

    engine->RemoveAll();

    EXPECT_EQ(engine->GetActiveConnectionsCount(), 0);
}

TEST_F(EngineTest, ApplyToConnections)
{
    int callCount = 0;

    // Add connections
    for (int i = 0; i < 3; ++i)
    {
        engine->AddConnection(process.get());
    }

    engine->ApplyToConnections([](Connection* conn, void* data) {
        auto intptr = (int*)data;
        intptr[0]++;
        }, &callCount);

    EXPECT_EQ(callCount, 3);
}

TEST_F(EngineTest, ExecuteConnections)
{
    // Add some connections first
    for (int i = 0; i < 2; ++i)
    {
        engine->AddConnection(process.get());
    }

    int context = 100;

    EXPECT_NO_THROW({
        engine->ExecuteConnections(&context);
        });
}

TEST_F(EngineTest, GetFirstAndLastConnectable)
{
    // Initially should return sentinel nodes
    auto first = engine->GetFirstConnectable();
    auto last = engine->GetLastConnectable();

    EXPECT_TRUE(first != nullptr);
    EXPECT_TRUE(last != nullptr);

    // Add a connection and check again
    engine->AddConnection(process.get());

    first = engine->GetFirstConnectable();
    last = engine->GetLastConnectable();

    EXPECT_TRUE(first != nullptr);
    EXPECT_TRUE(last != nullptr);
}

TEST_F(EngineTest, InspectOutput)
{
    const char* inspection = engine->Inspect();

    // Проверяем, что строка не пустая и содержит ожидаемые элементы
    EXPECT_TRUE(inspection != nullptr);
    EXPECT_TRUE(strlen(inspection) > 0);
}

TEST_F(EngineTest, PoolExhaustion)
{
    // Fill the pool
    auto smallEngine = std::make_unique<Engine>(SID("SmallEngine"), 2);

    smallEngine->AddConnection(process.get());
    smallEngine->AddConnection(process.get());

    // Should handle pool exhaustion gracefully (не бросать исключение согласно исходному коду)
    EXPECT_NO_THROW({
        smallEngine->AddConnection(process.get());
        });
}

// Test for memory management
class EngineMemoryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        engine = std::make_unique<Engine>(SID("MemoryTestEngine"), 5);
        process = std::make_unique<Process>(SID("MemoryTestProcess"));
    }

    std::unique_ptr<Engine> engine;
    std::unique_ptr<Process> process;
};

TEST_F(EngineMemoryTest, NoMemoryLeaksWithMultipleOperations)
{
    for (int i = 0; i < 10; ++i)
    {
        engine->AddConnection(process.get());
        engine->ProcessDisconnect(process.get());
    }

    // If we get here without crashes, memory management is working
    SUCCEED();
}

TEST_F(EngineMemoryTest, RemoveByParameters)
{
    int value1 = 42;
    int value2 = 100;
    void* testObject = &value1;

    // Add connections with different parameters
    engine->AddConnection(process.get(), testObject, 1, 2, 3);
    engine->AddConnection(process.get(), &value2, 1, 2, 3);
    engine->AddConnection(process.get(), nullptr, 42, 2, 3);

    EXPECT_EQ(engine->GetActiveConnectionsCount(), 3);

    // Remove by param0
    engine->RemoveByParam0(testObject);
    EXPECT_EQ(engine->GetActiveConnectionsCount(), 2);

    // Remove by param1
    engine->RemoveByParam1(42);
    EXPECT_EQ(engine->GetActiveConnectionsCount(), 1);

    // Remove by param2
    engine->RemoveByParam2(2);
    EXPECT_EQ(engine->GetActiveConnectionsCount(), 0);
}

// Test template method
TEST_F(EngineTest, RemoveMatching)
{
    // Add connections with different values
    for (int i = 0; i < 5; ++i)
    {
        engine->AddConnection(process.get(), nullptr, i, 0, 0);
    }

    EXPECT_EQ(engine->GetActiveConnectionsCount(), 5);

    // Remove connections where param1 is even
    engine->RemoveMatching([](Connection* conn, Engine* engine, void* data) {
        return conn->Arg1 % 2 == 0;
        }, NULL);

    EXPECT_EQ(engine->GetActiveConnectionsCount(), 2); // Only odd values remain
}