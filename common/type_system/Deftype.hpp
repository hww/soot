#pragma once

#include "common/soot/Object.hpp"
#include "common/type_system/State.hpp"
#include "common/type_system/TypeSpec.hpp"
#include "common/type_system/TypeSystem.hpp"

struct DeftypeResult {
    TypeSpec  type;
    Type     *type_info = nullptr;
    TypeFlags flags;
    bool      create_runtime_type = true;
};

TypeSpec parse_typespec(TypeSystem *type_system, const soot::Object &src);

DeftypeResult parse_deftype(const soot::Object &deftype, TypeSystem *ts,
                            soot::EnvironmentMap *constants = nullptr);