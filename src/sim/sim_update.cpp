#include "sentinel/sim/sim_update.hpp"
#include <cmath>

void sim_update(
    SimWorld& world,
    SimPlayer& p,
    float dt,
    float throttle,
    float strafe,
    float yaw,
    float pitch
) {
    world.time += dt;

    constexpr float MOVE_SPEED   = 6.0f;
    constexpr float STRAFE_SPEED = 5.0f;
    constexpr float TURN_RATE    = 1.8f;
    constexpr float PITCH_RATE   = 1.4f;
    constexpr float VERT_SPEED   = 4.0f;

    // Orientation
    p.yaw   += yaw   * TURN_RATE  * dt;
    p.pitch += pitch * PITCH_RATE * dt;

    if (p.pitch >  1.2f) p.pitch =  1.2f;
    if (p.pitch < -1.2f) p.pitch = -1.2f;

    const float cy = std::cos(p.yaw);
    const float sy = std::sin(p.yaw);

    // Forward / backward
    p.x += cy * throttle * MOVE_SPEED * dt;
    p.z += sy * throttle * MOVE_SPEED * dt;

    // Strafe
    p.x += -sy * strafe * STRAFE_SPEED * dt;
    p.z +=  cy * strafe * STRAFE_SPEED * dt;

    // Vertical
    p.y += throttle * VERT_SPEED * dt;
}
