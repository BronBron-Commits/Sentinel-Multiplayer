#include "sentinel/sim/sim_update.hpp"

void sim_update(
    SimWorld& world,
    SimPlayer& player,
    float dt,
    float throttle
) {
    world.time += dt;

    constexpr float SPEED = 2.0f;
    player.x += throttle * SPEED * dt;
}
