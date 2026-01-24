#pragma once

struct SimWorld {
    float time = 0.0f;
};

struct SimPlayer {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float yaw   = 0.0f;
    float pitch = 0.0f;
};

void sim_update(
    SimWorld& world,
    SimPlayer& player,
    float dt,
    float throttle,
    float yaw,
    float pitch
);
