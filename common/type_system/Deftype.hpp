#pragma once

#include "script/Object.hpp"
#include "type_system/State.hpp"
#include "type_system/TypeSpec.hpp"
#include "type_system/TypeSystem.hpp"

struct DeftypeResult {
    TypeSpec  type;
    Type     *type_info = nullptr;
    TypeFlags flags;
    bool      create_runtime_type = true;
};

TypeSpec parse_typespec(TypeSystem *type_system, const script::Object &src);

DeftypeResult parse_deftype(const script::Object &deftype, TypeSystem *ts,
                            script::EnvironmentMap *constants = nullptr);