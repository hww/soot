#include "common/carbon/vm/StackFrame.hpp"
#include "common/carbon/lib/Export.hpp"
#include "common/carbon/files/Export.hpp"

using namespace carbon::lib;
using namespace carbon::files;

namespace carbon::vm {

    StackFrame::StackFrame(FunctionDesc* functionDesc, std::shared_ptr<StackFrame>  parent,
        FrameType frame_type, StringId name)
        : name(name)
        , frame_type(frame_type)
        , byte_code(functionDesc)
        , code_ptr(functionDesc ? functionDesc->get_code_ptr() : nullptr)
        , data_ptr(functionDesc ? functionDesc->get_data_ptr() : nullptr)
        , parent(parent)
        , pc(0)
        , argc(0)
        , ret_num(0) {
        initialize_registers();
    }


} // namespace vm