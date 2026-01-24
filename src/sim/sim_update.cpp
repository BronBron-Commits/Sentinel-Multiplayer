#include "sentinel/sim/sim_update.hpp"
#include <cmath>

void sim_update(
    SimWorld& world,
    SimPlayer& player,
    float dt,
    float throttle,
    float yaw,
    float pitch
) {
    world.time += dt;

    constexpr float TURN_RATE  = 1.8f;  // rad/sec
    constexpr float PITCH_RATE = 1.2f;
    constexpr float SPEED      = 6.0f;

    player.yaw   += yaw   * TURN_RATE  * dt;
    player.pitch += pitch * PITCH_RATE * dt;

    // Clamp pitch
    if (player.pitch >  1.2f) player.pitch =  1.2f;
    if (player.pitch < -1.2f) player.pitch = -1.2f;

    const float cx = std::cos(player.yaw);
    const float sx = std::sin(player.yaw);

    player.x += cx * throttle * SPEED * dt;
    player.z += sx * throttle * SPEED * dt;
}
