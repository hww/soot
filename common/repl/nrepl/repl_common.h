#pragma once

#include <cstdint>

// Общие определения для ReplServer и ReplClient
enum class ReplMessageType : uint32_t {
    PING = 0,
    EVAL = 10,
    SHUTDOWN = 20
};

struct ReplMessageHeader {
    uint32_t length;
    ReplMessageType type;
};