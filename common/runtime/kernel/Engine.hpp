#pragma once

#include "common/runtime/ForwardDeclarations.hpp"
#include "common/runtime/kernel/Connectable.hpp"
#include "common/runtime/kernel/Connection.hpp"
#include "common/runtime/lib/StringId.hpp"

using namespace runtime::lib;

namespace runtime { namespace kernel {

class Process;

/**
 * Manages connections between processes and provides execution infrastructure for runtime operations.
 *
 * The Engine class implements a sophisticated connection system that allows processes to register
 * with engines and enables engines to iterate through connected processes.
 */
class Engine
{
public:
    /// Name identifier for this engine instance
    StringId Name;

    /// Current number of active connections
    int Length;

    /// Frame count when engine was last executed
    int FrameCount;

    /// Time when engine was last executed
    float Time;

    /// Sentinel node marking the start of the active connections list
    Connectable AliveList;

    /// Sentinel node marking the end of the active connections list
    Connectable AliveListEnd;

    /// Sentinel node marking the start of the inactive connections pool
    Connectable DeadList;

    /// Sentinel node marking the end of the inactive connections pool
    Connectable DeadListEnd;

    /// Pre-allocated pool of connection objects for efficient memory management
    Connection** Data;

    /// Number of connections in the pool
    int DataSize;

    /**
     * Initializes a new Engine instance with the specified name and connection pool size.
     * @param name Identifier for this engine
     * @param size Number of connections to pre-allocate in the pool
     */
    Engine(StringId name, int size);

    /**
     * Cleans up engine resources
     */
    ~Engine();

    /**
     * Gets the first valid connection in the active connections list.
     * @return The first active connection, or null if no connections are active
     */
    Connectable* GetFirstConnectable() const;

    /**
     * Gets the sentinel node marking the end of the active connections list.
     * @return The end sentinel node (not a valid connection)
     */
    Connectable* GetLastConnectable() const;

    /**
     * Gets the number of currently active connections
     */
    int GetActiveConnectionsCount() const { return Length; }

    /**
     * Gets the total allocated size of the connection pool
     */
    int GetPoolCapacity() const { return DataSize; }

    /**
     * Applies a function to all active connections in the engine.
     * @param action Action to perform on each connection
     */
    void ApplyToConnections(void (*action)(Connection*, void* data), void* data);

    /**
     * Applies a function to all active connections in reverse order.
     * @param action Action to perform on each connectable
     */
    void ApplyToConnectionsReversed(void (*action)(Connectable*, void* data), void* data);

    /**
     * Executes all active connections in reverse order.
     * @param context Context object passed to each connection's function
     */
    void ExecuteConnections(void* context);

    /**
     * Executes all active connections and moves dead connections to the inactive pool.
     * @param context Context object passed to each connection's function
     */
    void ExecuteConnectionsAndMoveToDead(void* context);

    /**
     * Executes connections only if the engine hasn't been executed this frame.
     * @param context Context object passed to each connection's function
     */
    void ExecuteConnectionsIfNeeded(void* context);

    /**
     * Applies a function to all connections belonging to a specific process.
     * @param process The target process
     * @param function Function to apply to each connection
     */
    void ConnectionProcessApply(Process* process, void (*function)(void*));

    /**
     * Establishes a new connection between this engine and the specified process.
     * @param process Process to connect
     * @param arg0 Function delegate or object reference
     * @param arg1 First integer parameter
     * @param arg2 Second integer parameter
     * @param arg3 Third integer parameter
     */
    void AddConnection(Process* process, void* arg0 = nullptr, int arg1 = 0, int arg2 = 0, int arg3 = 0);

    /**
     * Disconnects all connections for the specified process.
     * @param process Process to disconnect
     */
    void ProcessDisconnect(Process* process);

    /**
     * Removes all connections from the specified process that belong to this engine.
     * @param process Process to remove connections from
     */
    void RemoveFromProcess(Process* process);

    /**
     * Predicate for filtering processes.
     */
    using ConnectionFilterPredicate = bool(*)(Connection* connection, Engine* engine, void* data);

    /**
     * Removes connections that match the specified predicate function.
     * @param predicate Function that returns true for connections to remove
     */
    void RemoveMatching(ConnectionFilterPredicate predicate, void* data);

    /**
     * Removes all active connections from the engine.
     */
    void RemoveAll();

    /**
     * Removes all connections where Arg0 matches the specified value.
     * @param value Value to match against Arg0
     */
    void RemoveByParam0(void* value);

    /**
     * Removes all connections where Arg1 matches the specified value.
     * @param value Value to match against Arg1
     */
    void RemoveByParam1(int value);

    /**
     * Removes all connections where Arg2 matches the specified value.
     * @param value Value to match against Arg2
     */
    void RemoveByParam2(int value);

    /**
     * Provides detailed inspection information about the Engine instance.
     * @return Formatted string containing engine state and connection information
     */
    const char* Inspect() const;
};

}} // namespace vm::runtime