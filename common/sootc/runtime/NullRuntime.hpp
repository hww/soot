// sootc/runtime/NullRuntime.hpp
#pragma once

#include "sootc/runtime/Runtime.hpp"

namespace sootc {

    /// @brief A Runtime that does nothing.
    ///
    /// @details Used by `--compile-only` and by tests that want to
    ///          verify compilation but never actually execute the
    ///          resulting bytecode. It accepts every module and every
    ///          call, and always reports success.
    ///
    ///          Note: this is not the same as "ignore errors". The
    ///          point is that sootc can be driven through its normal
    ///          pipeline without depending on the presence of a VM.
    class NullRuntime : public Runtime {
    public:
        bool load_module(carbon::BinaryFile &&file) override {
            (void)file;
            return true;
        }

        RuntimeResult call(const std::string                  &name,
                           const std::vector<carbon::Variant> &args) override {
            (void)name;
            (void)args;
            RuntimeResult out;
            out.ok = true;
            return out;
        }

        std::string name() const override { return "null"; }

        bool has_symbol(const std::string &) const override { return true; }
    };

} // namespace sootc