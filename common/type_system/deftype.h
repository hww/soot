#pragma once

#include "script/object.h"
#include "type_spec.h"
#include "type_system.h"
#include "state.h"

struct DeftypeResult {
    TypeSpec type;
    Type* type_info = nullptr;
    TypeFlags flags;
    bool create_runtime_type = true;
};

TypeSpec parse_typespec(TypeSystem* type_system, const script::Object& src);
DeftypeResult parse_deftype(const script::Object& deftype,
    TypeSystem* ts,
    script::EnvironmentMap* constants = nullptr);