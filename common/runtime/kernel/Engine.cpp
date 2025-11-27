#include "common/runtime/kernel/Engine.hpp"
#include "common/runtime/kernel/Process.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace runtime { namespace kernel {

// Static buffer for string representations
static char s_StringBuffer[2048];

Engine::Engine(StringId name, int size)
    : Name(name), Length(0), FrameCount(0), Time(0.0f),
      AliveList(this), AliveListEnd(this), DeadList(this), DeadListEnd(this),
      DataSize(size)
{
    // Allocate connection pool
    Data = new Connection*[size];
    for (int i = 0; i < size; i++)
        Data[i] = new Connection();

    // Link alive list sentinels
    AliveList.Next0 = &AliveListEnd;
    AliveListEnd.Prev0 = &AliveList;

    // Link dead list to connection pool
    DeadList.Next0 = Data[0];
    DeadListEnd.Prev0 = Data[DataSize - 1];

    // Connect pool elements in a circular doubly-linked list
    Data[0]->Prev0 = &DeadList;
    Data[DataSize - 1]->Next0 = &DeadListEnd;

    // Link all pool elements together
    for (int i = 0; i < DataSize - 1; i++)
    {
        Data[i]->Next0 = Data[i + 1];
        Data[i + 1]->Prev0 = Data[i];
    }
}

Engine::~Engine()
{
    // Clean up connection pool
    for (int i = 0; i < DataSize; i++)
        delete Data[i];
    delete[] Data;

    // No need to clean up sentinel nodes - they are automatically destroyed
}

Connectable* Engine::GetFirstConnectable() const
{
    return AliveList.Next0 != &AliveListEnd ? AliveList.Next0 : nullptr;
}

Connectable* Engine::GetLastConnectable() const
{
    return const_cast<Connectable*>(&AliveListEnd);
}

void Engine::ApplyToConnections(void (*action)(Connection*, void*), void* data)
{
    Connectable* current = AliveList.Next0;
    while (current != nullptr && current != &AliveListEnd)
    {
        Connectable* next = current->Next0;
        Connection* connection = static_cast<Connection*>(current);
        if (connection != nullptr)
            action(connection, data);
        current = next;
    }
}

void Engine::ApplyToConnectionsReversed(void (*action)(Connectable*, void*), void* data)
{
    Connectable* current = AliveListEnd.Prev0;
    while (current != nullptr && current != &AliveList)
    {
        Connectable* previous = current->Prev0;
        action(current, data);
        current = previous;
    }
}


void Engine::ExecuteConnections(void* context)
{
    Connection* current = static_cast<Connection*>(AliveListEnd.Prev0);
    while (current != nullptr && current != &AliveList)
    {
        Connection* previous = static_cast<Connection*>(current->Prev0);
        ConnectionFunction function = current->GetFunction();
        if (function != nullptr)
            function(current->Arg1, current->Arg2, current->Arg3, context);
        current = previous;
    }
}

void Engine::ExecuteConnectionsAndMoveToDead(void* context)
{
    Connection* current = static_cast<Connection*>(AliveListEnd.Prev0);
    while (current != nullptr && current != &AliveList)
    {
        Connection* previous = static_cast<Connection*>(current->Prev0);
        ConnectionFunction function = current->GetFunction();
        EEngineResult result = EER_None;

        if (function != nullptr)
            result = function(current->Arg1, current->Arg2, current->Arg3, context);

        if (result == EER_Dead)
            current->MoveToDead();

        current = previous;
    }
}

void Engine::ExecuteConnectionsIfNeeded(void* context)
{
    ExecuteConnections(context);
}

void Engine::ConnectionProcessApply(Process* process, void (*function)(void*))
{
    if (process == nullptr) return;

    Connectable* item = process->ConnectionList.Next1;
    while (item != nullptr)
    {
        function(item);
        item = item->Next1;
    }
}

void Engine::AddConnection(Process* process, void* arg0, int arg1, int arg2, int arg3)
{
    if (process == nullptr)
        return;

    if (DeadList.Next0 == &DeadListEnd)
        return; // Connection pool exhausted

    Connectable* connectable = DeadList.Next0;
    Connection* connection = static_cast<Connection*>(connectable);

    // Configure connection parameters
    connection->Arg0 = arg0;
    connection->Arg1 = arg1;
    connection->Arg2 = arg2;
    connection->Arg3 = arg3;

    // Remove from dead list
    DeadList.Next0 = connectable->Next0;
    connectable->Next0->Prev0 = &DeadList;

    // Add to alive list (insert at beginning)
    connectable->Next0 = AliveList.Next0;
    connectable->Next0->Prev0 = connectable;
    connectable->Prev0 = &AliveList;
    AliveList.Next0 = connectable;

    // Add to process list (insert at beginning)
    connectable->Next1 = process->ConnectionList.Next1;
    if (connectable->Next1 != nullptr)
        connectable->Next1->Prev1 = connectable;
    connectable->Prev1 = &process->ConnectionList;
    process->ConnectionList.Next1 = connectable;

    Length++;
}

void Engine::ProcessDisconnect(Process* process)
{
    if (process == nullptr) return;

    Connection* item = static_cast<Connection*>(process->ConnectionList.Next1);
    while (item != nullptr)
    {
        Connection* next = static_cast<Connection*>(item->Next1);
        item->MoveToDead();
        item = next;
    }
}

void Engine::RemoveFromProcess(Process* process)
{
    if (process == nullptr) return;

    Connection* item = static_cast<Connection*>(process->ConnectionList.Next1);
    while (item != nullptr)
    {
        Connection* next = static_cast<Connection*>(item->Next1);
        if (item->BelongsToEngine(this))
            item->MoveToDead();
        item = next;
    }
}


void Engine::RemoveMatching(ConnectionFilterPredicate predicate, void* data)
{
    Connectable* current = AliveList.Next0;
    while (current != nullptr && current != &AliveListEnd)
    {
        Connectable* next = current->Next0;
        Connection* connection = static_cast<Connection*>(current);
        if (connection != nullptr && predicate(connection, this, data))
            connection->MoveToDead();
        current = next;
    }
}

void Engine::RemoveAll()
{
    Connectable* current = AliveList.Next0;
    while (current != nullptr && current != &AliveListEnd)
    {
        Connectable* next = current->Next0;
        Connection* connection = static_cast<Connection*>(current);
        if (connection != nullptr)
            connection->MoveToDead();
        current = next;
    }
}

void Engine::RemoveByParam0(void* value)
{
    RemoveMatching([](Connection* connection, Engine* engine, void* data) {
        return connection->Arg0 == data;
    }, value);
}

void Engine::RemoveByParam1(int value)
{
    RemoveMatching([](Connection* connection, Engine* engine, void* data) {
        return connection->Arg1 == *((int*)data);
    }, &value);
}

void Engine::RemoveByParam2(int value)
{
    RemoveMatching([](Connection* connection, Engine* engine, void* data) {
        return connection->Arg2 == *((int*)data);
    }, &value);
}

const char* Engine::Inspect() const
{
    snprintf(s_StringBuffer, sizeof(s_StringBuffer),
        "<Engine name=%s frameCount=%d length=%d>\n"
        "  AliveList: %s\n"
        "  AliveListEnd: %s\n"
        "  DeadList: %s\n"
        "  DeadListEnd: %s\n"
        "</Engine>",
        Name, FrameCount, Length,
        AliveList.Inspect(),
        AliveListEnd.Inspect(),
        DeadList.Inspect(),
        DeadListEnd.Inspect());

    return s_StringBuffer;
}

}} // namespace vm::runtime