#include "gtest/gtest.h"
#include "TestUtils.hpp"


#include "carbon/Export.hpp"

using namespace carbon;
using namespace carbon;
using namespace carbon;
using namespace carbon;
using namespace carbon;


TEST(StackFrame, Construction) {
    StackFrame frame;
    EXPECT_EQ(frame.pc, 0);
    EXPECT_EQ(frame.argc, 0);
    EXPECT_EQ(frame.ret_num, 0);
    EXPECT_EQ(frame.parent, nullptr);
}


TEST(StackFrame, RegisterAccess) {
    StackFrame frame;

    // Test local register access
    frame.get_local(0) = Variant(42);
    EXPECT_EQ(frame.get_local(0).get_i64(), 42);

    // Test argument register access
    frame.get_argument(0) = Variant(100);
    EXPECT_EQ(frame.get_argument(0).get_i64(), 100);

    // Test bounds checking - should throw for out-of-bounds
    EXPECT_THROW(frame.get_register(MAX_REGISTERS), std::exception);

    // MAX_REGISTERS - 1 should work (last valid register)
    EXPECT_NO_THROW(frame.get_register(MAX_REGISTERS - 1));

    // Argument bounds - MAX_ARGS should throw
    EXPECT_THROW(frame.get_argument(MAX_ARGS), std::exception);

    // MAX_ARGS - 1 should work (last valid argument)
    EXPECT_NO_THROW(frame.get_argument(MAX_ARGS - 1));

    // Local bounds - MAX_LOCALS should throw
    EXPECT_THROW(frame.get_local(MAX_LOCALS), std::exception);

    // MAX_LOCALS - 1 should work (last valid local)
    EXPECT_NO_THROW(frame.get_local(MAX_LOCALS - 1));
}

TEST(StackFrame, ArgumentCopying) {
    StackFrame caller;
    StackFrame callee;

    // Set up arguments in caller
    caller.get_argument(0) = Variant(10);
    caller.get_argument(1) = Variant(20);

    // Copy arguments to callee
    callee.setup_call(2, 25); // 2 arguments, return to r25
    callee.copy_arguments_from(caller);

    EXPECT_EQ(callee.get_argument(0).get_i64(), 10);
    EXPECT_EQ(callee.get_argument(1).get_i64(), 20);
    EXPECT_EQ(callee.argc, 2);
    EXPECT_EQ(callee.ret_num, 25);
}

TEST(StackFrame, FrameManagement) {
    Instruction dummy_code[10];

    FunctionDesc FunctionDesc;
    auto parent = create_stack_frame(&FunctionDesc, nullptr);
    auto child = push_stack_frame(&FunctionDesc, parent);

    EXPECT_EQ(child->parent, parent);

    auto popped = pop_stack_frame(child);
    EXPECT_EQ(popped, parent);

    destroy_stack_frame(parent);
}

TEST(StackFrame, StringRepresentation) {
    StackFrame frame;
    EXPECT_FALSE(frame.to_string().empty());
}