#pragma once
#include <cstdint>

struct SimPlayer {
    uint32_t id;
    float x;
    float y;
    float z;
};

struct SimWorld {
    float time = 0.0f;
};
