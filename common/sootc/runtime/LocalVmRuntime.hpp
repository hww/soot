#pragma once

#include "carbon/Carbon.hpp"
#include "sootc/runtime/Runtime.hpp"

namespace sootc {

    /// @brief Runtime backend that executes code in-process, via a
    ///        long-lived Carbon instance owned by this object.
    ///
    /// Unlike the old implementation, which talked directly to
    /// carbon::Globals::inst() and a bare VirtualMachine, this class
    /// owns a full Carbon context. That means:
    ///
    ///   - module load/unload goes through Carbon::load_module /
    ///     unload_module, so SsTypes are registered in the GC and
    ///     symbols live in Carbon's Globals, not in a process-wide
    ///     singleton;
    ///
    ///   - each LocalVmRuntime is isolated: two of them do not share
    ///     a symbol table, a heap, or a process list. This is what
    ///     makes unit tests safe.
    class LocalVmRuntime : public Runtime {
    public:
        LocalVmRuntime();
        ~LocalVmRuntime() override;

        [[nodiscard]] std::string name() const override { return "local-vm"; }

        bool          load_module(carbon::BinaryFile &&file) override;
        RuntimeResult call(const std::string                  &name,
                           const std::vector<carbon::Variant> &args) override;
        
        bool          has_symbol(const std::string &name) const override {
            return m_carbon.globals() &&
                   m_carbon.globals()->find_symbol_ptr(carbon::StringId(name)) != nullptr;
        }

    private:
        carbon::Carbon m_carbon;
    };

} // namespace sootc