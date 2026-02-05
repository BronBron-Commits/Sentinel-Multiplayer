#include "warthog_controller.hpp"
#include <cmath>
#include <algorithm>

constexpr float MAX_SPEED = 14.0f;
constexpr float ACCEL = 10.0f;
constexpr float TURN_RATE = 2.8f;
constexpr float CAM_UP = 2.0f;

void warthog_init(WarthogState& w)
{
    w = {};
    w.y = 0.5f;
}

void warthog_update(
    WarthogState& w,
    const ControlState& ctl,
    float dt
)
{
    // steering = mouse X ONLY
    w.yaw += ctl.look_dx * TURN_RATE * dt;

    float target_speed = ctl.forward * MAX_SPEED;
    w.speed += (target_speed - w.speed) * ACCEL * dt;

    float fx = std::cos(w.yaw);
    float fz = std::sin(w.yaw);

    w.x += fx * w.speed * dt;
    w.z += fz * w.speed * dt;
}

void warthog_update_camera(
    const WarthogState& w,
    Camera& cam,
    float cam_distance
)
{
    float fx = std::cos(w.yaw);
    float fz = std::sin(w.yaw);

    cam.target = {
        w.x + fx,
        w.y + 0.8f,
        w.z + fz
    };

    cam.pos = {
        w.x - fx * cam_distance,
        w.y + CAM_UP,
        w.z - fz * cam_distance
    };
}
