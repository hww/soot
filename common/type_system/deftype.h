#pragma once

/*!
 * @file deftype.h
 * Parser for the GOAL "deftype" form.
 * This is used both in the compiler and in the decompiler for the type definition file.
 */

#include "state.h"
#include "type_spec.h"
#include "type_system.h"
#include "script/object.h"

struct DeftypeResult {
    TypeFlags flags;
    TypeSpec type;
    Type* type_info = nullptr;
    bool create_runtime_type = true;
};

DeftypeResult parse_deftype(const script::Object& deftype,
    TypeSystem* ts,
    DefinitionMetadata* symbol_metadata = nullptr);
TypeSpec parse_typespec(TypeSystem* type_system, const script::Object& src);