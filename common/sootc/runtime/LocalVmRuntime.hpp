// sootc/runtime/LocalVmRuntime.hpp
#pragma once

#include "sootc/runtime/Runtime.hpp"

#include "carbon/vm/VirtualMachine.hpp"

namespace sootc {

    /// @brief In-process Carbon VM as a Runtime.
    ///
    /// @details Wraps `carbon::VirtualMachine` and `carbon::Globals`.
    ///          This is exactly the logic that used to live inline in
    ///          `Compiler::compile_and_report` and the old `:run`
    ///          REPL command, extracted into a Runtime so that the
    ///          rest of sootc does not need to care where code is
    ///          executed.
    ///
    ///          Loaded modules are registered in `carbon::Globals`. The
    ///          VM is reused across calls, but its frame-lifetime
    ///          allocators are purged after each call, matching the
    ///          existing `vm.purge()` behaviour.
    class LocalVmRuntime : public Runtime {
        carbon::VirtualMachine m_vm;

    public:
        LocalVmRuntime() = default;

        bool load_module(carbon::BinaryFile &&file) override;

        RuntimeResult call(const std::string                  &name,
                           const std::vector<carbon::Variant> &args) override;

        std::string name() const override { return "local-vm"; }
    };

} // namespace sootc