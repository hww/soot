#include <gtest/gtest.h>
#include "common/carbon/kernel/Connectable.hpp"
#include "common/carbon/kernel/Engine.hpp"
#include "common/carbon/kernel/Process.hpp"
#include "common/carbon/kernel/Engine.hpp"

using namespace carbon;

using namespace vm;

    class ConnectionTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            engine = std::make_shared<Engine>(SID("TestEngine"), 10);
            process = std::make_shared<Process>(SID("TestProcess"));
        }

        std::shared_ptr<Engine> engine;
        std::shared_ptr<Process> process;
    };

    TEST_F(ConnectionTest, ConstructorInitialization)
    {
        Connection connection;

        EXPECT_TRUE(connection.Owner != nullptr);
        EXPECT_EQ(connection.Arg1, 0);
        EXPECT_EQ(connection.Arg2, 0);
        EXPECT_EQ(connection.Arg3, 0);
        EXPECT_EQ(connection.Arg0, nullptr);
    }

    TEST_F(ConnectionTest, FunctionStorage)
    {
        Connection connection;
        bool functionCalled;
        auto testFunction = [](int a, int b, int c, void* ctx) -> EEngineResult {
            *((bool*)ctx) = true;

            return EEngineResult::EER_None;
            };

        connection.SetFunction(testFunction);
        auto retrievedFunction = connection.GetFunction();

        EXPECT_TRUE(retrievedFunction != nullptr);

        // Test function execution
        retrievedFunction(1, 2, 3, &functionCalled);
        EXPECT_TRUE(functionCalled);
    }

    TEST_F(ConnectionTest, ArgumentStorage)
    {
        Connection connection;

        int testObject = 42;
        connection.Arg0 = &testObject;
        connection.Arg1 = 100;
        connection.Arg2 = 200;
        connection.Arg3 = 300;

        EXPECT_EQ(connection.Arg0, testObject);
        EXPECT_EQ(connection.Arg1, 100);
        EXPECT_EQ(connection.Arg2, 200);
        EXPECT_EQ(connection.Arg3, 300);
    }


    TEST_F(ConnectionTest, MoveToDeadWithoutEngine)
    {
        Connection connection;

        // Should not crash when no engine is available
        Connection* result = connection.MoveToDead();
        EXPECT_EQ(result, &connection);
    }

    TEST_F(ConnectionTest, InspectFormat)
    {
        Connection connection;
        connection.Arg1 = 1;
        connection.Arg2 = 2;
        connection.Arg3 = 3;

        std::string result = connection.Inspect();

        EXPECT_NE(result.find("<Connection"), std::string::npos);
        EXPECT_NE(result.find("args=["), std::string::npos);
        EXPECT_NE(result.find("1 2 3"), std::string::npos);
    }
