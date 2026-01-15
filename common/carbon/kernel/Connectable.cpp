#include "common/carbon/kernel/Connectable.hpp"
#include "common/carbon/kernel/Process.hpp"
#include "common/carbon/kernel/Engine.hpp"
#include "common/carbon/kernel/Connection.hpp"
#include <cstdio>
#include <cstring>

namespace runtime { namespace kernel {

    // Static buffer for string representations
    static char s_StringBuffer[1024];

    Connectable::Connectable(void* owner)
        : Owner(owner), Next0(nullptr), Prev0(nullptr), Next1(nullptr), Prev1(nullptr)
    {
    }

    const char* Connectable::OwnerToString() const
    {
        if (Owner == nullptr) return "null";

        // These would be properly implemented when classes are available
        // if (Owner is Process) return process->name;
        // if (Owner is Engine) return engine->Name;
        // if (Owner is Connection) return connection hash

        return "unknown";
    }

    const char* Connectable::ToString() const
    {
        snprintf(s_StringBuffer, sizeof(s_StringBuffer),
            "<Connectable hash=%p owner='%s' next0='%s' prev0='%s' next1='%s' prev1='%s'>",
            this,
            OwnerToString(),
            Next0 ? Next0->OwnerToString() : "null",
            Prev0 ? Prev0->OwnerToString() : "null",
            Next1 ? Next1->OwnerToString() : "null",
            Prev1 ? Prev1->OwnerToString() : "null");

        return s_StringBuffer;
    }

    const char* Connectable::Inspect() const
    {
        return ToString();
    }

}} // namespace vm::runtime