

#include "NativeAllocators.hpp"

namespace carbon {

    Variant allocate_vector(u32 argc, const Variant *argv) {
        if (argc < 2) return Variant(0);
        return Variant(0);
    }


    void NativeAllocators::initialize_builtins() {
        // Basic I/O
        register_function("allocate-vector", allocate_vector);
    }
}; // namespace carbon