// sootc/runtime/LocalVmRuntime.cpp
#include "sootc/runtime/LocalVmRuntime.hpp"

#include "carbon/file/Globals.hpp"
#include "common/util/Log.hpp"

namespace sootc {

    bool LocalVmRuntime::load_module(carbon::BinaryFile &&file) {
        // Register every named entry of the binary in the process-wide
        // symbol table. After this call, `Globals::find_symbol_ptr`
        // can resolve any function or data item the binary exports.
        return carbon::Globals::inst().load_module(std::move(file));
    }

    RuntimeResult LocalVmRuntime::call(const std::string                  &name,
                                       const std::vector<carbon::Variant> &args) {
        RuntimeResult out;

        // Resolve the function name to a ScriptLambda pointer. The
        // pointer is owned by the BinaryFile that was loaded earlier;
        // it remains valid for the lifetime of the Globals registry.
        void *fn_ptr = carbon::Globals::inst().find_symbol_ptr(carbon::StringId(name));
        if (fn_ptr == nullptr) {
            out.ok = false;
            out.error = fmt::format("function '{}' not found in Globals", name);
            return out;
        }

        auto *lambda = reinterpret_cast<carbon::ScriptLambda *>(fn_ptr);

        // Execute the lambda with the given positional arguments.
        // Arg 0 goes into r24, arg 1 into r25, etc. — see the VM
        // calling convention in VirtualMachine::execute_function.
        try {
            out.value = m_vm.execute_function(lambda, carbon::RunMode::Run, args);
            out.ok = true;
        } catch (const std::exception &e) {
            out.ok = false;
            out.error = e.what();
            return out;
        }

        // Every object allocated during this call is no longer
        // reachable from the runtime after we return. Purge the
        // frame-lifetime allocators. This mirrors the old
        // `vm.purge()` at the end of the `:run` REPL command.
        m_vm.purge();

        return out;
    }

} // namespace sootc