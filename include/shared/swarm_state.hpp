#pragma once
#include <vector>
#include <cstdint>

enum class FormationMode {
    LINE  = 1,
    ORBIT = 3
};

struct SwarmAgent {
    uint32_t id;
    float x;
    float y;
    float vx;
    float vy;
};

struct SwarmState {
    uint64_t tick = 0;
    FormationMode formation = FormationMode::LINE;
    std::vector<SwarmAgent> agents;
};
