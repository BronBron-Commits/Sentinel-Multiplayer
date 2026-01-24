#pragma once
#include <cstdint>

struct Snapshot {
    uint32_t player_id;   // authoritative identity
    float x;
    float y;
    float z;
};
