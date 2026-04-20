#pragma once


// utils/ByteUtils.hpp
#include <cstddef>
#include <memory>

namespace carbon {
    struct aligned_deleter {
        void operator()(std::byte* p) const noexcept {
            ::operator delete[](p, std::align_val_t(64));
        }
    };
    
    using byte_uptr = std::unique_ptr<std::byte[], aligned_deleter>;
    
    inline byte_uptr make_aligned_buffer(size_t size) {
        return byte_uptr(
            static_cast<std::byte*>(::operator new[](size, std::align_val_t(64))),
            aligned_deleter()
        );
    }
}