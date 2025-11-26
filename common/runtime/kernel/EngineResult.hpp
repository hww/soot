#pragma once

namespace vm
{
    /// <summary>
    /// Represents the result state of engine operations.
    /// </summary>
    enum class EEngineResult
    {
        /// <summary>No specific result</summary>
        None,

        /// <summary>Indicates the engine or connection should be terminated</summary>
        Dead
    };
} // namespace vm
