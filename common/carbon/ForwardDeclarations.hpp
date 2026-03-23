#pragma once

namespace runtime {

    // Forward declarations
    namespace lib {
        class Variant;
        template<typename T> struct Ptr;
    }

    namespace files {
        struct Descriptor;
        struct BinaryFile;
        struct FunctionDesc;
        struct Definition;
        struct SourceLocation;
        class BinaryFilePool;
        class BinaryFileBuilder;
        struct DciFile;
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
        struct StateDefinition;
        class DeadPool;
        class EntityActor;
    }
}