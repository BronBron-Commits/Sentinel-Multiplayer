#pragma once
#include <cstdint>

// Discrete world events (sent once)
enum class NetEventType : uint8_t {
    FIREWORK_LAUNCH = 1
};

struct NetEvent {
    NetEventType type;
    float x;
    float y;
    float z;
    uint32_t seed;
};

