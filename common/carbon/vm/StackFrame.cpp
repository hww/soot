#include "common/carbon/vm/StackFrame.hpp"
#include "common/carbon/lib/Export.hpp"
#include "common/carbon/files/Export.hpp"
#include "common/carbon/files/BinaryFile.hpp"

using namespace runtime::lib;
using namespace runtime::files;

namespace runtime::vm {

    StackFrame::StackFrame(FunctionDesc* FunctionDesc, StackFrame* parent,
        FrameType frame_type, StringId name)
        : name(name), frame_type(frame_type), byte_code(FunctionDesc),
        code_ptr(FunctionDesc ? FunctionDesc->get_code_ptr() : nullptr),
        data_ptr(FunctionDesc ? FunctionDesc->get_data_ptr() : nullptr),
        parent_ptr(parent), pc(0), argc(0), ret_num(0) {
        initialize_registers();
    }


} // namespace vm