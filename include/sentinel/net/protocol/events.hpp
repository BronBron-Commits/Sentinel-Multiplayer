#pragma once
#include <cstdint>

enum class EventType : uint8_t {
    FIREWORK = 1,
};

struct Event {
    EventType type;
    uint32_t player_id;
    float x, y, z;
    uint32_t seed;
};
