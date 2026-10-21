#pragma once

namespace carbon {
    // Forward declarations
    class Variant;
    template<typename T> struct Ptr;

    struct SsDeclarationList;
    struct SsDeclaration;
    struct StateScript;
    struct SsOptions;
    struct SymbolArray;
    struct SsState;
    struct SsTrackGroup;
    struct SsOnBlock;
    struct SsTrack;
    struct SsLambda;
    struct ScriptLambda;

    class Module;
    class ModuleRegistry;
    class ModuleManager;

    struct StackFrame;
    class VirtualMachine;
    class StateFrame;

    class Process;
    class Scheduler;
    class Engine;
    class Connection;
    class Connectable;
    class DeadPool;
    class EntityActor;
    struct EventMessage;
}