#pragma once
#include <cstdint>
#include <vector>

struct PlayerState {
    uint32_t id;
    float x, y, z;
    float yaw, pitch, roll;
};

struct Snapshot {
    uint32_t tick = 0;
    std::vector<PlayerState> players;
};
