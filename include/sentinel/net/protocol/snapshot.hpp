#pragma once
#include <cstdint>

struct Snapshot {
    uint32_t player_id;
    float x;
    float y;
    float z;
    float yaw;
    uint32_t tick;
};
