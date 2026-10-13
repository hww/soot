#include "sootc/runtime/LocalVmRuntime.hpp"

#include "carbon/file/BinaryFile.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/vm/VirtualMachine.hpp"
#include "util/Log.hpp"

namespace sootc {

    LocalVmRuntime::LocalVmRuntime() {
        carbon::CarbonConfig config;
        // The compiler does not want a sidbase of its own; it uses
        // whatever StringIdManager has already loaded. So leave
        // config.sidbase_path empty.
        m_carbon.initialize(config);
    }

    LocalVmRuntime::~LocalVmRuntime() { m_carbon.shutdown(); }

    bool LocalVmRuntime::load_module(carbon::BinaryFile &&file) {
        const std::string logical_name = file.m_path.stem().string();
        return m_carbon.load_module_from_binary(logical_name, std::move(file));
    }

    RuntimeResult LocalVmRuntime::call(const std::string                  &name,
                                       const std::vector<carbon::Variant> &args) {
        RuntimeResult out;

        carbon::CallResult r = m_carbon.call(name, args);
        out.ok = r.ok;
        out.value = r.value;
        out.error = r.error;
        return out;
    }

} // namespace sootc