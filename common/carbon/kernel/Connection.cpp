#include "common/carbon/kernel/Connection.hpp"
#include "common/carbon/kernel/Engine.hpp"
#include "common/carbon/kernel/Process.hpp"
#include <cstdio>
#include <cstring>

namespace carbon::kernel {

// Static buffer for string representations
static char s_StringBuffer[1024];

Connection::Connection()
    : Connectable(nullptr), Arg0(nullptr), Arg1(0), Arg2(0), Arg3(0)
{
    Owner = this;
}

ConnectionFunction Connection::GetFunction() const
{
    return reinterpret_cast<ConnectionFunction>(Arg0);
}

void Connection::SetFunction(ConnectionFunction func)
{
    Arg0 = reinterpret_cast<void*>(func);
}

Engine* Connection::GetEngine() const
{
    const int iterationLimit = 1000;
    int iterationsRemaining = iterationLimit;

    Connectable* current = const_cast<Connectable*>(static_cast<const Connectable*>(this));

    while (current != nullptr && iterationsRemaining > 0)
    {
        if (current->Owner != nullptr)
        {
            // This would be properly implemented when Engine class is available
            // if (current->Owner is Engine) return static_cast<Engine*>(current->Owner);
        }

        current = current->Prev0;
        iterationsRemaining--;
    }

    return nullptr;
}

Process* Connection::GetProcess() const
{
    const int iterationLimit = 1000;
    int iterationsRemaining = iterationLimit;

    Connectable* current = const_cast<Connectable*>(static_cast<const Connectable*>(this));

    while (current != nullptr && iterationsRemaining > 0)
    {
        if (current->Owner != nullptr)
        {
            // This would be properly implemented when Process class is available
            // if (current->Owner is Process) return static_cast<Process*>(current->Owner);
        }

        current = current->Prev1;
        iterationsRemaining--;
    }

    return nullptr;
}

bool Connection::BelongsToEngine(Engine* engine) const
{
    return engine == GetEngine();
}

bool Connection::BelongsToProcess(Process* process) const
{
    return GetProcess() == process;
}

Connection* Connection::MoveToDead()
{
    Engine* engine = GetEngine();
    if (engine == nullptr)
    {
        // Log error: Engine not found
        return this;
    }

    // Remove from engine's alive list
    if (Prev0 != nullptr) Prev0->Next0 = Next0;
    if (Next0 != nullptr) Next0->Prev0 = Prev0;

    // Remove from process list
    if (Prev1 != nullptr) Prev1->Next1 = Next1;
    if (Next1 != nullptr) Next1->Prev1 = Prev1;

    // This would connect to Engine's MoveToDead implementation
    // Simplified version for now

    return this;
}

const char* Connection::Inspect() const
{
    snprintf(s_StringBuffer, sizeof(s_StringBuffer),
        "<Connection owner='%s' next0='%s' prev0='%s' next1='%s' prev1='%s' args=[%p %d %d %d]>",
        OwnerToString(),
        Next0 ? Next0->OwnerToString() : "null",
        Prev0 ? Prev0->OwnerToString() : "null",
        Next1 ? Next1->OwnerToString() : "null",
        Prev1 ? Prev1->OwnerToString() : "null",
        Arg0, Arg1, Arg2, Arg3);

    return s_StringBuffer;
}

} // namespace vm::runtime