#pragma once

#include "type_system.h"
#include "object.h"

EnumType* parse_defenum(const script::Object& defenum,
    TypeSystem* ts,
    DefinitionMetadata* symbol_metadata = nullptr);