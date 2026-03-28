#pragma once

namespace carbon {

    // Forward declarations
    namespace lib {
        class Variant;
        template<typename T> struct Ptr;
    }

    namespace files {
        struct Descriptor;
        struct BinaryFile;
        struct Definition;
        struct SourceLocation;
        struct DciFile;
        struct FunctionDesc;
        struct MethodDef; 
        struct StateDesc;
        struct TypeDesc;
        class BinaryFilePool;
        class BinaryFileBuilder;
    }

    namespace modules {
        class Module;
        class ModuleRegistry;
        class ModuleManager;
    }

    namespace vm {
        struct Instruction;
        struct StackFrame;
        class VirtualMachine;
        class StateFrame;
    }

    namespace kernel {
        class Process;
        class Scheduler;
        class Engine;
        class Connection;
        class Connectable;
        class DeadPool;
        class EntityActor;
        struct EventMessage;
    }
}