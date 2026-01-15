#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/kernel/Connectable.hpp"

namespace runtime { namespace kernel {

class Process;
class Engine;

/**
 * Represents the result state of engine operations.
 */
enum EEngineResult
{
    EER_None,   ///< No specific result
    EER_Dead    ///< Indicates the engine or connection should be terminated
};

/**
 * Function signature for connection execution
 */
typedef EEngineResult (*ConnectionFunction)(int, int, int, void*);

/**
 * Represents a connection between processes in the runtime engine, storing arguments
 * and providing methods for connection management and traversal.
 *
 * Connections are specialized Connectable nodes that carry up to four arguments (one object and three integers)
 * and provide functionality for moving between engine lists and process-specific lists.
 */
class Connection : public Connectable
{
public:
    /// First argument (typically a function delegate or object reference)
    void* Arg0;

    /// Second argument (typically an integer parameter)
    int Arg1;

    /// Third argument (typically an integer parameter)
    int Arg2;

    /// Fourth argument (typically an integer parameter)
    int Arg3;

    /**
     * Initializes a new Connection instance with self-ownership.
     */
    Connection();

    /**
     * Gets the function delegate stored in Arg0.
     */
    ConnectionFunction GetFunction() const;

    /**
     * Sets the function delegate stored in Arg0.
     */
    void SetFunction(ConnectionFunction func);

    /**
     * Traverses the connection hierarchy to find the owning Engine.
     * @return The Engine that owns this connection, or null if not found within iteration limits
     */
    Engine* GetEngine() const;

    /**
     * Traverses the connection hierarchy to find the owning Process.
     * @return The Process that owns this connection, or null if not found within iteration limits
     */
    Process* GetProcess() const;

    /**
     * Determines whether this connection belongs to the specified engine.
     * @param engine The engine to check against
     * @return True if this connection belongs to the specified engine, false otherwise
     */
    bool BelongsToEngine(Engine* engine) const;

    /**
     * Determines whether this connection belongs to the specified process.
     * @param process The process to check against
     * @return True if this connection belongs to the specified process, false otherwise
     */
    bool BelongsToProcess(Process* process) const;

    /**
     * Moves this connection from the alive list to the dead list in the owning engine.
     * @return The current connection instance for method chaining
     */
    Connection* MoveToDead();

    /**
     * Provides detailed inspection information about the Connection instance.
     * @return A comprehensive string representation including connection pointers and argument values
     */
    const char* Inspect() const override;
};

}} // namespace vm::runtime