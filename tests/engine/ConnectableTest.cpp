#include <gtest/gtest.h>
#include "common/carbon/Export.hpp"

using namespace carbon::kernel;
using namespace carbon::lib;

using namespace carbon::vm;

    class ConnectableTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            process = new Process(SID("TestProcess"));
            engine = new Engine(SID("TestEngine"), 10);
        }
        void TearDown() override {
            delete process;
            delete engine;
        }
        Process* process;
        Engine* engine;
    };

    TEST_F(ConnectableTest, ConstructorWithOwner)
    {
        auto connectable = std::make_unique<Connectable>(process);
        EXPECT_TRUE(connectable->Owner != nullptr);
        EXPECT_EQ(connectable->Next0, nullptr);
        EXPECT_EQ(connectable->Prev0, nullptr);
        EXPECT_EQ(connectable->Next1, nullptr);
        EXPECT_EQ(connectable->Prev1, nullptr);
    }

    TEST_F(ConnectableTest, ConstructorWithoutOwner)
    {
        auto connectable = std::make_unique<Connectable>();
        EXPECT_EQ(connectable->Owner, nullptr);
    }

    TEST_F(ConnectableTest, OwnerToStringWithNullOwner)
    {
        Connectable connectable(nullptr);
        EXPECT_EQ(connectable.OwnerToString(), "null");
    }

    TEST_F(ConnectableTest, ToStringFormat)
    {
        Connectable connectable(process);
        std::string result = connectable.ToString();

        EXPECT_NE(result.find("<Connectable"), std::string::npos);
        EXPECT_NE(result.find("owner="), std::string::npos);
    }

    TEST_F(ConnectableTest, InspectEqualsToString)
    {
        Connectable connectable(process);
        EXPECT_EQ(connectable.Inspect(), connectable.ToString());
    }

    TEST_F(ConnectableTest, LinkedListLinking)
    {
        auto node1 = std::make_unique<Connectable>(process);
        auto node2 = std::make_unique<Connectable>(process);
        auto node3 = std::make_unique<Connectable>(process);

        // Create a simple linked list
        node1->Next0 = node2.get();
        node2->Prev0 = node1.get();
        node2->Next0 = node3.get();
        node3->Prev0 = node2.get();

        EXPECT_EQ(node1->Next0, node2.get());
        EXPECT_EQ(node2->Prev0, node1.get());
        EXPECT_EQ(node2->Next0, node3.get());
        EXPECT_EQ(node3->Prev0, node2.get());
    }
