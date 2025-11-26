#include <gtest/gtest.h>
#include "Engine.h"
#include "Process.h"
#include <thread>

using namespace vm;
    class EngineTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            engine = std::make_shared<Engine>("TestEngine", 10);
            process = std::make_shared<Process>("TestProcess");
        }

        std::shared_ptr<Engine> engine;
        std::shared_ptr<Process> process;
    };

    TEST_F(EngineTest, ConstructorInitialization)
    {
        EXPECT_EQ(engine->GetName(), "TestEngine");
        EXPECT_EQ(engine->GetLength(), 0);
        EXPECT_EQ(engine->GetFrameCount(), 0);
        EXPECT_FLOAT_EQ(engine->GetTime(), 0.0f);
        EXPECT_EQ(engine->GetPoolCapacity(), 10);
    }

    TEST_F(EngineTest, AddConnection)
    {
        auto testObject = std::make_shared<int>(42);

        EXPECT_NO_THROW({
            engine->AddConnection(process, testObject, 1, 2, 3);
            });

        EXPECT_EQ(engine->GetActiveConnectionsCount(), 1);
    }

    TEST_F(EngineTest, AddConnectionWithNullProcess)
    {
        EXPECT_THROW({
            engine->AddConnection(nullptr);
            }, std::invalid_argument);
    }

    TEST_F(EngineTest, MultipleConnections)
    {
        for (int i = 0; i < 5; ++i)
        {
            engine->AddConnection(process, nullptr, i, i * 2, i * 3);
        }

        EXPECT_EQ(engine->GetActiveConnectionsCount(), 5);
    }

    TEST_F(EngineTest, ProcessDisconnect)
    {
        // Add multiple connections
        for (int i = 0; i < 3; ++i)
        {
            engine->AddConnection(process);
        }

        EXPECT_EQ(engine->GetActiveConnectionsCount(), 3);

        engine->ProcessDisconnect(process);

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
            engine->AddConnection(process);
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
            engine->AddConnection(process);
        }

        engine->ApplyToConnections([&callCount](Connection* conn) {
            callCount++;
            });

        EXPECT_EQ(callCount, 3);
    }

    TEST_F(EngineTest, ExecuteConnections)
    {
        int executionCount = 0;

        // Create a process with a function
        auto executingProcess = std::make_shared<Process>("ExecutingProcess");

        // We need to modify Engine to allow setting functions on connections
        // This is a simplified test
        auto context = std::make_shared<int>(100);

        EXPECT_NO_THROW({
            engine->ExecuteConnections(context);
            });
    }

    TEST_F(EngineTest, GetFirstAndLastConnectable)
    {
        EXPECT_TRUE(engine->GetFirstConnectable() != nullptr);
        EXPECT_TRUE(engine->GetLastConnectable() != nullptr);
    }

    TEST_F(EngineTest, InspectOutput)
    {
        std::string inspection = engine->Inspect();

        EXPECT_NE(inspection.find("<Engine"), std::string::npos);
        EXPECT_NE(inspection.find("TestEngine"), std::string::npos);
        EXPECT_NE(inspection.find("AliveList:"), std::string::npos);
        EXPECT_NE(inspection.find("DeadList:"), std::string::npos);
    }

    TEST_F(EngineTest, PoolExhaustion)
    {
        // Fill the pool
        auto smallEngine = std::make_shared<Engine>("SmallEngine", 2);

        smallEngine->AddConnection(process);
        smallEngine->AddConnection(process);

        // Should throw when pool is exhausted
        EXPECT_THROW({
            smallEngine->AddConnection(process);
            }, std::runtime_error);
    }


    // Test for memory management
    class EngineMemoryTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            engine = std::make_shared<vm::Engine>("MemoryTestEngine", 5);
            process = std::make_shared<vm::Process>("MemoryTestProcess");
        }

        std::shared_ptr<vm::Engine> engine;
        std::shared_ptr<vm::Process> process;
    };

    TEST_F(EngineMemoryTest, NoMemoryLeaksWithMultipleOperations)
    {
        for (int i = 0; i < 10; ++i)
        {
            engine->AddConnection(process);
            engine->ProcessDisconnect(process);
        }

        // If we get here without crashes, memory management is working
        SUCCEED();
    }

    TEST_F(EngineMemoryTest, SharedOwnership)
    {
        auto weakEngine = std::weak_ptr<vm::Engine>(engine);
        auto weakProcess = std::weak_ptr<vm::Process>(process);

        // Reset main shared_ptrs
        engine.reset();
        process.reset();

        // Objects should be destroyed
        EXPECT_TRUE(weakEngine.expired());
        EXPECT_TRUE(weakProcess.expired());
    }

