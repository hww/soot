#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/soot/Object.hpp"

EnumType* parse_defenum(const soot::Object& defenum,
    TypeSystem* ts,
    DefinitionMetadata* symbol_metadata = nullptr);