#include "common/carbon/files/FunctionDesc.hpp"
#include "common/carbon/modules/Module.hpp"
#include "common/carbon/lib/Ptr.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "common/util/Formatter.hpp"
#include <cstddef>
#include <fmt/format.h>
#include <string>

using namespace carbon::lib;
using namespace carbon::vm;
using namespace util;

namespace carbon::files {

    
FunctionDesc::FunctionDesc() 
    : code_count(0)
    , data_size(0)
    , debug_count(0)
    , code_ptr(nullptr)
    , data_ptr(nullptr)
    , debug_ptr(nullptr)
    , owner_module(nullptr) {
}

u8* FunctionDesc::get_data_ptr() const {
    return data_ptr.get();
}

SourceLocation* FunctionDesc::get_debug_info() const {
    return debug_ptr.get();
}

bool FunctionDesc::has_debug_info() const {
    return debug_ptr != nullptr && debug_count > 0;
}

void FunctionDesc::relocate_pointers(bool to_memory, intptr_t delta, Module* module) {
    (void)to_memory;
    owner_module = module;


    if (code_ptr.offset) {
        code_ptr.offset += delta;
    }
    if (data_ptr.offset) {
        data_ptr.offset += delta;
    }
    if (debug_ptr.offset) {
        debug_ptr.offset += delta;
    }
    if (verbose) {
        lg::info("[FunctionDesc] relocate_pointers #code {:016X} #data {:016X} #debug {:016X}", 
            code_ptr.offset, data_ptr.offset, debug_ptr.offset);
    }
}

SourceLocation FunctionDesc::find_source_location(u32 instruction_ip) const {
    auto debug_info = get_debug_info();
    if (!debug_info) {
        return SourceLocation{ 0, 0, 0, 0 };
    }
    // Linear search through debug info to find matching instruction offset
    // In production this could be optimized with binary search if entries are sorted
    for (u32 i = 0; i < debug_count; ++i) {
        if (debug_info[i].start == instruction_ip) {
            return debug_info[i];
        }
    }
    // No debug info found for this instruction
    return SourceLocation{ 0, 0, 0, 0 };
}

std::string FunctionDesc::inspect() const {
    std::string result;

    result += Formatter::instance().format("FunctionDesc {{\n");
    Formatter::instance().inc_column(2);

    result += Formatter::instance().format("code_count: {}\n", code_count);
    result += Formatter::instance().format("data_size: {}\n", data_size);
    result += Formatter::instance().format("debug_count: {}\n", debug_count);
    result += Formatter::instance().format("code_ptr: {}\n", fmt::ptr(code_ptr.get()));
    result += Formatter::instance().format("data_ptr: {}\n", fmt::ptr(data_ptr.get()));
    result += Formatter::instance().format("debug_ptr: {}\n", fmt::ptr(debug_ptr.get()));
    result += Formatter::instance().format("owner_module: {}\n", fmt::ptr(owner_module));

    Formatter::instance().inc_column(2);

    // Только если code_ptr валидный и code_count > 0
    if (code_ptr.ptr && code_count > 0 && code_ptr.ptr != reinterpret_cast<Instruction*>(0x10)) {
        result +=  Formatter::instance().format("Code:\n");
        for (size_t i = 0; i < code_count; i++) {
            auto inst = code_ptr.ptr[i];
            result += Formatter::instance().format("[{}] {}\n", i, InstructionTable::instance().disassemble(inst));
        }
    } else if (code_count > 0) {
        result += Formatter::instance().format("Code: (invalid pointer, skipping)\n");
    }
    Formatter::instance().inc_column(-2);
    Formatter::instance().inc_column(-2);
    result += Formatter::instance().format("}}\n");
    return result;
}


} // namespace carbon::files
