#include "sootc/runtime/LocalVmRuntime.hpp"

#include "carbon/file/BinaryFile.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/vm/VirtualMachine.hpp"
#include "util/Log.hpp"

#include <atomic>

namespace sootc {

    LocalVmRuntime::LocalVmRuntime() {
        carbon::CarbonConfig config;
        if (!m_carbon.initialize(config)) {
            lg::warn("LocalVmRuntime: Carbon already active in this thread, reusing it");
            // m_carbon не инициализирован, но t_current уже указывает на другой Carbon.
            // Это плохо, но пока SootcTest использует s_carbon = Carbon::current(),
            // run() будет работать с тем Carbon, который создал LocalVmRuntime...
            // Нет, не будет. См. ниже.
        }
    }

    LocalVmRuntime::~LocalVmRuntime() { m_carbon.shutdown(); }

    bool LocalVmRuntime::load_module(carbon::BinaryFile &&file) {
        // Every load gets a unique logical name, so that consecutive REPL
        // inputs do not overwrite each other's ModuleRegistry entry. If
        // they did, the previous entry's BinaryFile would be freed while
        // Globals still held pointers into it — and functions defined in
        // an earlier input (like cvector-new) would silently disappear
        // from Globals the moment a later input (like test) was loaded.
        //
        // The counter is monotonic; a module is never reused for a
        // different file. Modules accumulate for the lifetime of the
        // runtime, which is exactly what we want for a REPL session.
        static std::atomic<u64> s_counter{0};
        const u64               n = s_counter.fetch_add(1, std::memory_order_relaxed);

        const std::string logical_name = fmt::format("{}_{}", file.m_path.stem().string(), n);

        return m_carbon.load_module_from_binary(logical_name, std::move(file));
    }

    bool LocalVmRuntime::run_top_level(const std::string &logical_name) {
        return m_carbon.run_top_level(logical_name);
    }

    RuntimeResult LocalVmRuntime::call(const std::string                  &name,
                                       const std::vector<carbon::Variant> &args) {
        RuntimeResult out;

        carbon::CallResult r = m_carbon.call(name, args);
        out.ok = r.ok;
        out.value = r.value;
        out.error = r.error;
        out.vm_error = r.vm_error;
        return out;
    }

} // namespace sootc