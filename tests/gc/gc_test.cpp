#include <gtest/gtest.h>

#include "gc/GC.hpp"
#include "gc/TypeRegistry.hpp"
#include "lib/StringId.hpp"
#include <vm/StackFrame.hpp>

using namespace carbon;
using namespace carbon::gc;

// ============================================================================
// Test types — регистрируем в реестре перед тестами.
// ============================================================================

namespace {

    // --- Тип "int32" — скаляр, без полей. ---
    // Используется как простой объект в тестах.

    // --- Тип "cell" — структура с одним указателем. ---
    // Используется для проверки рекурсивного mark.

    void setup_test_types() {
        // int32
        TypeDescriptor td_int{};
        td_int.name = SID("int32");
        td_int.size = 4;
        td_int.align = 4;
        td_int.flags = TYPE_IS_SCALAR;
        td_int.num_fields = 0;
        td_int.fields = nullptr;
        type_registry().register_type(td_int);

        // cell — структура с одним указателем cell*
        TypeDescriptor td_cell{};
        td_cell.name = SID("cell");
        td_cell.size = 8;
        td_cell.align = 8;
        td_cell.flags = TYPE_IS_REFERENCE;
        td_cell.num_fields = 1;

        FieldDesc fd{};
        fd.type = SID("cell");
        fd.offset = 0;
        fd.size = 8;
        fd.flags = 0; // одиночный указатель
        fd.count = 0;

        type_registry().register_type_with_fields(td_cell, &fd, 1);
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

TEST_F(GCTest, InitializeShutdown) {
    // Просто проверяем, что initialize/shutdown не падают.
    EXPECT_TRUE(true);
}

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
    gc::release(p); // не должно падать

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

    // Объект должен быть жив.
    auto h = header_of(p);
    EXPECT_NE(h->pass_cycle, 0u);
}

TEST_F(GCTest, CollectAfterMarkKeepsObject) {
    void *p = gc::alloc(SID("int32"));
    ASSERT_NE(p, nullptr);

    const auto before = gc::live_objects();

    // Mark + collect.
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

    // Помечаем только outer.
    gc::start_pass();
    gc::mark_object(outer);
    while (!gc::sweep_pass(1024)) {}
    gc::finish_pass();

    // Оба живы.
    EXPECT_EQ(gc::live_objects(), 2u);
}

TEST_F(GCTest, SweepFreesUnmarked) {
    void *a = gc::alloc(SID("cell"));
    void *b = gc::alloc(SID("cell"));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Помечаем только a.
    gc::start_pass();
    gc::mark_object(a);
    while (!gc::sweep_pass(1024)) {}
    gc::finish_pass();

    // b должен быть освобождён. Теперь live = 1.
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
    gc::mark_object(&local); // не должно падать
    gc::finish_pass();
    EXPECT_TRUE(true);
}

TEST_F(GCTest, FreeDuringPassIsSafe) {
    void *p = gc::alloc(SID("int32"));
    ASSERT_NE(p, nullptr);

    gc::start_pass();
    gc::release(p); // явно освободили во время прохода
    while (!gc::sweep_pass(1024)) {}
    gc::finish_pass();

    // Не должно падать, объект должен быть учтён как освобождённый.
    EXPECT_TRUE(true);
}

TEST_F(GCTest, StackFrameRootsKeepObjectAlive) {
    auto frame = std::make_shared<StackFrame>();

    void *obj = gc::alloc(SID("cell"));
    ASSERT_NE(obj, nullptr);
    frame->registers[0] = Variant(obj, RuntimeType::Pointer);

    // Ручной mark, имитируем Kernel::mark_frame_recursive.
    gc::start_pass();
    const u64 pass = gc::current_pass();

    if (frame->mark_id != pass) {
        frame->mark_id = pass;
        for (auto &reg : frame->registers) {
            if (reg.is_ptr() && gc::is_in_heap(reg.get_ptr())) { gc::mark_object(reg.get_ptr()); }
        }
    }

    while (!gc::sweep_pass(1024)) {}
    gc::finish_pass();

    // Объект должен быть жив.
    EXPECT_EQ(gc::live_objects(), 1u);
}

TEST_F(GCTest, StackFrameNotMarkedFreesObject) {
    auto frame = std::make_shared<StackFrame>();

    void *obj = gc::alloc(SID("cell"));
    ASSERT_NE(obj, nullptr);
    frame->registers[0] = Variant(obj, RuntimeType::Pointer);

    // НЕ помечаем фрейм.
    gc::collect();

    // Объект должен быть удалён.
    EXPECT_EQ(gc::live_objects(), 0u);
}
