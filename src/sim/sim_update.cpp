#include "sentinel/sim/sim_update.hpp"

void sim_update(SimWorld& world, SimPlayer& player, float dt) {
    world.time += dt;

    // TEMP: simple motion so we see something
    player.x += 0.5f * dt;
}
