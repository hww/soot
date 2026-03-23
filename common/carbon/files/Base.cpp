#include "common/CommonTypes.hpp"
#include "common/carbon/files/Base.hpp"

using namespace runtime::lib;

namespace runtime::files {
    // =============================================================================
    // FunctionDescError Implementation
    // =============================================================================

    FunctionDescError::FunctionDescError(const std::string& msg) : message(msg) {}

    const char* FunctionDescError::what() const noexcept {
        return message.c_str();
    }

    // =============================================================================
    // SourceLocation Implementation  
    // =============================================================================

    std::string SourceLocation::to_string() const {
        return fmt::format("SourceLocation(ip:{:04x}, line:{}, file:{})",
            offset, line, file);
    }


} // end of namespace