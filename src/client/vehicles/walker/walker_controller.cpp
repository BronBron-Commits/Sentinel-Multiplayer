#include "walker_controller.hpp"
#include "input/control_system.hpp"
#include "render/camera.hpp"

#include <cmath>

constexpr float MOVE_SPEED = 8.0f;
constexpr float TURN_SPEED = 2.8f;
constexpr float CAM_UP = 3.2f;

void walker_init(WalkerState& w)
{
    w.x = 0.0f;
    w.y = 0.0f;
    w.z = 0.0f;

    w.yaw = 0.0f;

    w.walk_phase = 0.0f;
    w.walk_speed = 0.0f;
}

void walker_update(
    WalkerState& w,
    const ControlState& ctl,
    float dt
)
{
    // --- Yaw (same as warthog steering) ---
    w.yaw += ctl.strafe * TURN_SPEED * dt;

    // --- Forward movement ---
    float forward = ctl.forward;

    float dx = std::cos(w.yaw) * forward * MOVE_SPEED * dt;
    float dz = std::sin(w.yaw) * forward * MOVE_SPEED * dt;

    w.x += dx;
    w.z += dz;

    // --- Walk animation drive ---
    w.walk_speed = std::fabs(forward);

    w.walk_phase += w.walk_speed * 6.0f * dt;

    if (w.walk_phase > 6.28318f)
        w.walk_phase -= 6.28318f;
}

void walker_update_camera(
    const WalkerState& w,
    Camera& cam,
    float cam_distance
)
{
    float fwd_x = std::cos(w.yaw);
    float fwd_z = std::sin(w.yaw);

    cam.target = {
        w.x,
        w.y + 1.6f,
        w.z
    };

    cam.pos = {
        w.x - fwd_x * cam_distance,
        w.y + CAM_UP,
        w.z - fwd_z * cam_distance
    };
}
