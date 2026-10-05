#include "StoreGlobalNode.hpp"
#include "FunctionNode.hpp"
#include "vm/Instructions.hpp"

namespace sootc {

void StoreGlobalNode::emit(FunctionNode &fn) {
        (void)fn;
        // TODO: emit StoreSymbol when opcode exists.
        // For now, just make sure the symbol name lands in the constant pool,
        // so we can at least see it in the listing.
        if (m_value) { m_value->emit(fn); }
        fn.add_constant(static_cast<u64>(StringId(m_name).value), FunctionNode::ConstKind::STRING);
    }

} // namespace sootc