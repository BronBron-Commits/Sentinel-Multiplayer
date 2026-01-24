#pragma once

struct SimWorld {
    float time = 0.0f;
};

struct SimPlayer {
    float x = 0.0f;
};

void sim_update(
    SimWorld& world,
    SimPlayer& player,
    float dt,
    float throttle
);
