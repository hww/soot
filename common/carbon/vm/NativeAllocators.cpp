#include "NativeAllocators.hpp"

#include "common/carbon/vm/NativeFunc.hpp"
#include "common/util/Log.hpp"



namespace carbon {

    // =========================================================================
    // Native functions
    // =========================================================================
    //
    // Each function receives raw argument bytes from the VM. The convention
    // matches every other native function in this codebase:
    //     argv[0] = number of elements to allocate (currently ignored).
    //     return  = pointer to the first allocated element, or 0 on failure.
    //
    // The pools are owned by the VirtualMachine, not by these free functions.
    // This file wires the free functions to the pools through a process-wide
    // singleton accessor that the VM installs at startup.

    namespace {
        NativeAllocators *g_allocators = nullptr;
    }

    void set_global_allocators(NativeAllocators *allocators) { g_allocators = allocators; }

    Variant allocate_i64(u32 argc, const Variant *argv) {
        (void)argc;
        (void)argv;
        if (!g_allocators) return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        i64 *ptr = g_allocators->allocate_i64_slot();
        return Variant(static_cast<void *>(ptr), RuntimeType::Pointer);
    }

    Variant allocate_f32(u32 argc, const Variant *argv) {
        (void)argc;
        (void)argv;
        if (!g_allocators) return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        f32 *ptr = g_allocators->allocate_f32_slot();
        return Variant(static_cast<void *>(ptr), RuntimeType::Pointer);
    }

    Variant allocate_sid(u32 argc, const Variant *argv) {
        (void)argc;
        (void)argv;
        if (!g_allocators) return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        sid64 *ptr = g_allocators->allocate_sid_slot();
        return Variant(static_cast<void *>(ptr), RuntimeType::Pointer);
    }

    Variant allocate_vector(u32 argc, const Variant *argv) {
        (void)argc;
        (void)argv;
        if (!g_allocators) return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        vector *ptr = g_allocators->allocate_vector_slot();
        std::memset(ptr, 0, sizeof(vector));
        return Variant(static_cast<void *>(ptr), RuntimeType::Pointer);
    }

    Variant allocate_cvector(u32 argc, const Variant *argv) {
        (void)argc;
        (void)argv;
        if (!g_allocators) return Variant(static_cast<void *>(nullptr), RuntimeType::Pointer);
        cvector *ptr = g_allocators->allocate_cvector_slot();
        return Variant(static_cast<void *>(ptr), RuntimeType::Pointer);
    }

    // =========================================================================
    // Registry
    // =========================================================================

    void NativeAllocators::initialize_builtins() {
        // Remember this instance so that the free native functions can reach
        // its pools. There is exactly one VirtualMachine in the process, and
        // its NativeAllocators is the one that owns the pools.
        g_allocators = this;

        auto &reg = NativeFunctionRegistry::get_instance();
        reg.register_function("allocate-i64", &allocate_i64);
        reg.register_function("allocate-f32", &allocate_f32);
        reg.register_function("allocate-sid", &allocate_sid);
        reg.register_function("allocate-vector", &allocate_vector);
        reg.register_function("allocate-cvector", &allocate_cvector);

        lg::info("Initialized {} allocator native functions", 3);
    }

    void NativeAllocators::clear_all() noexcept {
        m_i64_pool.clear();
        m_f32_pool.clear();
        m_sid_pool.clear();
        m_sid_pool.clear();
    }

} // namespace carbon