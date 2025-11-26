#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/script/Object.hpp"

EnumType* parse_defenum(const script::Object& defenum,
    TypeSystem* ts,
    DefinitionMetadata* symbol_metadata = nullptr);