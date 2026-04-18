#include "common/carbon/vm/StackFrame.hpp"
#include "common/carbon/lib/Export.hpp"
#include "common/carbon/file/Export.hpp"
#include "file/DCScript.hpp"
#include "vm/Instructions.hpp"

using namespace carbon;
using namespace carbon;

namespace carbon {

    StackFrame::StackFrame(ScriptLambda* functionDesc, std::shared_ptr<StackFrame>  parent,
        FrameType frame_type, StringId name)
        : name(name)
        , frame_type(frame_type)
        , byte_code(functionDesc)
        , code_ptr(functionDesc ? (Instruction*)functionDesc->m_pInstruction : nullptr)
        , data_ptr(functionDesc ? functionDesc->m_pSymbols : nullptr)
        , parent(parent)
        , pc(0)
        , argc(0)
        , ret_num(0) {
        initialize_registers();
    }


} // namespace vm