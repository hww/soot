#include <gtest/gtest.h>

#include "gc/GC.hpp"
#include "gc/GCHeader.hpp"
#include "gc/TypeRegistry.hpp"
#include "lib/StringId.hpp"
#include "lib/Variant.hpp"
#include "vm/StackFrame.hpp"

using namespace carbon;
using namespace carbon::gc;

// ============================================================================
// Test types
// ============================================================================
//
// Two types are registered for the tests:
//
//   int32 -- scalar, no pointers. Used as a plain payload.
//   cell  -- reference type with one pointer at offset 0. Used to
//            exercise the recursive mark path.
//
// With the new TypeDescriptor there is no FieldDesc array. Instead, the
// type provides a mark delegate that walks its own pointers, and the GC
// just calls it.

namespace {

    // Mark delegate for `cell`. The cell has exactly one pointer at
    // offset 0. It forwards that pointer to the visitor, which is the
    // GC's mark_object trampoline.
    void mark_cell(void *payload, MarkVisitor visitor, void *user) {
        void *inner = *reinterpret_cast<void **>(payload);
        if (inner) visitor(0, inner, 0, user);
    }

    void destroy_cell(void * /*payload*/) {}

    void setup_test_types() {
        auto &reg = type_registry();

        // int32 -- scalar.
        {
            TypeDescriptor td{};
            td.name = SID("int32");
            td.size = 4;
            td.align = 4;
            td.num_ptr_fields = 0;
            td.flags = TYPE_IS_SCALAR;
            td.ptr_fields = nullptr;
            td.mark = nullptr;
            td.destroy = nullptr;
            reg.register_type(td);
        }

        // cell -- reference with one pointer at offset 0.
        {
            TypeDescriptor td{};
            td.name = SID("cell");
            td.size = 8;
            td.align = 8;
            td.num_ptr_fields = 1;
            td.flags = TYPE_IS_REFERENCE;

            // PtrField carries the offset. The mark delegate below is what
            // actually walks it. A future version of the GC could use
            // ptr_fields directly, but for now the delegate is authoritative.
            static PtrField pf{};
            pf.type_index = reg.find_index(SID("cell"));
            pf.offset = 0;
            pf.array_count = 0;
            pf.flags = 0;

            td.ptr_fields = &pf;
            td.mark = &mark_cell;
            td.destroy = &destroy_cell;
            reg.register_type(td);
        }
    }

} // namespace

// ============================================================================
// Fixture
// ============================================================================

class GCTest : public ::testing::Test {
protected:
    void SetUp() override {
        gc::initialize(/*initial_mb=*/4, /*max_mb=*/64);
        setup_test_types();
    }

    void TearDown() override {
        gc::shutdown();
        type_registry().clear();
    }
};

// ============================================================================
// Tests
// ============================================================================

TEST_F(GCTest, InitializeShutdown) { EXPECT_TRUE(true); }

TEST_F(GCTest, AllocReturnsValidPointer) {
    void *p = gc::alloc(SID("int32"));
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(gc::is_in_heap(p));

    auto s = gc::stats();
    EXPECT_GE(s.total_allocated, 1u);
}

TEST_F(GCTest, AllocFreeNoCrash) {
    void *p = gc::alloc(SID("int32"));
    ASSERT_NE(p, nullptr);

    gc::release(p);

    auto s = gc::stats();
    EXPECT_GE(s.total_freed, 1u);
}

TEST_F(GCTest, FreeIsIdempotent) {
    void *p = gc::alloc(SID("int32"));
    ASSERT_NE(p, nullptr);

    gc::release(p);
    gc::release(p); // must not crash

    EXPECT_TRUE(true);
}

TEST_F(GCTest, CollectWithoutRootsFreesObject) {
    void *p = gc::alloc(SID("int32"));
    ASSERT_NE(p, nullptr);

    const auto before = gc::live_objects();
    EXPECT_GE(before, 1u);

    gc::collect();

    const auto after = gc::live_objects();
    EXPECT_LT(after, before);
}

TEST_F(GCTest, MarkObjectKeepsItAlive) {
    void *p = gc::alloc(SID("int32"));
    ASSERT_NE(p, nullptr);

    gc::start_pass();
    gc::mark_object(p);
    while (!gc::sweep_pass(1024)) {}
    gc::finish_pass();

    // Object must be alive. pass_cycle is a bitfield inside flags, so
    // use the accessor instead of the (removed) field.
    auto h = header_of(p);
    EXPECT_NE(gc_get_pass_cycle(h->flags), 0u);
}

TEST_F(GCTest, CollectAfterMarkKeepsObject) {
    void *p = gc::alloc(SID("int32"));
    ASSERT_NE(p, nullptr);

    const auto before = gc::live_objects();

    gc::start_pass();
    gc::mark_object(p);
    while (!gc::sweep_pass(1024)) {}
    gc::finish_pass();

    const auto after = gc::live_objects();
    EXPECT_EQ(before, after);
}

TEST_F(GCTest, RecursiveMark) {
    // cell -> cell -> nullptr
    void *inner = gc::alloc(SID("cell"));
    void *outer = gc::alloc(SID("cell"));
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(outer, nullptr);

    // outer->field = inner
    *reinterpret_cast<void **>(outer) = inner;

    // Mark only outer. The delegate must follow the pointer into inner.
    gc::start_pass();
    gc::mark_object(outer);
    while (!gc::sweep_pass(1024)) {}
    gc::finish_pass();

    EXPECT_EQ(gc::live_objects(), 2u);
}

TEST_F(GCTest, SweepFreesUnmarked) {
    void *a = gc::alloc(SID("cell"));
    void *b = gc::alloc(SID("cell"));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Mark only a.
    gc::start_pass();
    gc::mark_object(a);
    while (!gc::sweep_pass(1024)) {}
    gc::finish_pass();

    // b must be freed.
    EXPECT_EQ(gc::live_objects(), 1u);
}

TEST_F(GCTest, StatsAreSane) {
    for (int i = 0; i < 10; ++i) { (void)gc::alloc(SID("int32")); }
    auto s = gc::stats();
    EXPECT_EQ(s.total_allocated, 10u);
    EXPECT_GE(s.live_objects, 10u);
}

TEST_F(GCTest, IsInHeapRejectsForeign) {
    int local = 0;
    EXPECT_FALSE(gc::is_in_heap(&local));
    EXPECT_FALSE(gc::is_in_heap(nullptr));
}

TEST_F(GCTest, MarkObjectForeignIsNoop) {
    int local = 0;
    gc::start_pass();
    gc::mark_object(&local); // must not crash
    gc::finish_pass();
    EXPECT_TRUE(true);
}

TEST_F(GCTest, FreeDuringPassIsSafe) {
    void *p = gc::alloc(SID("int32"));
    ASSERT_NE(p, nullptr);

    gc::start_pass();
    gc::release(p); // explicit release during a pass
    while (!gc::sweep_pass(1024)) {}
    gc::finish_pass();

    EXPECT_TRUE(true);
}

TEST_F(GCTest, StackFrameRootsKeepObjectAlive) {
    auto frame = std::make_shared<StackFrame>();

    void *obj = gc::alloc(SID("cell"));
    ASSERT_NE(obj, nullptr);
    frame->registers[0] = Variant(obj, RuntimeType::Pointer);

    // Manual mark, mimicking Kernel::mark_frame_recursive.
    gc::start_pass();
    const u32 pass = gc::current_pass();

    if (frame->mark_id != pass) {
        frame->mark_id = pass;
        for (auto &reg : frame->registers) {
            if (reg.is_ptr() && gc::is_in_heap(reg.get_ptr())) { gc::mark_object(reg.get_ptr()); }
        }
    }

    while (!gc::sweep_pass(1024)) {}
    gc::finish_pass();

    EXPECT_EQ(gc::live_objects(), 1u);
}

TEST_F(GCTest, StackFrameNotMarkedFreesObject) {
    auto frame = std::make_shared<StackFrame>();

    void *obj = gc::alloc(SID("cell"));
    ASSERT_NE(obj, nullptr);
    frame->registers[0] = Variant(obj, RuntimeType::Pointer);

    // Do not mark the frame.
    gc::collect();

    EXPECT_EQ(gc::live_objects(), 0u);
}