#pragma once

#include "type_system/TypeSystem.hpp"
#include "script/Object.hpp"

EnumType* parse_defenum(const script::Object& defenum,
    TypeSystem* ts,
    DefinitionMetadata* symbol_metadata = nullptr);