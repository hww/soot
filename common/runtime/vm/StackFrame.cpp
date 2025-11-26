#include "common/runtime/vm/StackFrame.hpp"
#include "common/runtime/lib/Export.hpp"
#include "common/runtime/files/Export.hpp"
#include "common/runtime/files/BinaryFile.hpp"

using namespace runtime::lib;
using namespace runtime::files;

namespace runtime::vm {

    StackFrame::StackFrame(ByteCode* bytecode, StackFrame* parent,
        FrameType frame_type, StringId name)
        : name(name), frame_type(frame_type), byte_code(bytecode),
        code_ptr(bytecode ? bytecode->get_code_ptr() : nullptr),
        data_ptr(bytecode ? bytecode->get_data_ptr() : nullptr),
        parent_ptr(parent), pc(0), argc(0), ret_num(0) {
        initialize_registers();
    }


} // namespace vm