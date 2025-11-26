#pragma once

#include "common/runtime/ForwardDeclarations.hpp"

namespace runtime { namespace kernel {

    class Process;
    class Engine;
    class Connection;

    /**
     * Represents a node in a doubly-linked list structure used for managing connections within the runtime engine.
     *
     * Connectable objects participate in two separate linked list hierarchies:
     * 1. Engine-wide connection lists (next0/prev0) - managed by the engine for tracking active/inactive connections
     * 2. Process-specific connection lists (next1/prev1) - organized per process with termination on both ends
     */
    class Connectable
    {
    public:
        /// Reference to the owning object of this connectable node
        void* Owner;

        /// Next node in the engine's connection management list
        Connectable* Next0;

        /// Previous node in the engine's connection management list
        Connectable* Prev0;

        /// Next node in the process-specific connection list
        Connectable* Next1;

        /// Previous node in the process-specific connection list
        Connectable* Prev1;

        /**
         * Initializes a new Connectable instance with the specified owner.
         * @param owner The object that owns this connectable node
         */
        Connectable(void* owner);

        /**
         * Converts the owner reference to a human-readable string representation.
         * @return String representation of the owner based on its actual type
         */
        const char* OwnerToString() const;

        /**
         * Returns a string that represents the current Connectable instance.
         * @return A formatted string containing the hash code, owner reference, and all connection pointers
         */
        const char* ToString() const;

        /**
         * Provides detailed inspection information about the Connectable instance.
         * @return A comprehensive string representation suitable for debugging and inspection
         */
        virtual const char* Inspect() const;
    };

}} // namespace vm::runtime