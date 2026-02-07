#include "walker_controller.hpp"
#include "input/control_system.hpp"
#include "render/camera.hpp"
#include "render/render_terrain.hpp"

#include <cmath>

// ------------------------------------------------------------
// Tuning
// ------------------------------------------------------------
constexpr float MOVE_SPEED = 8.0f;
constexpr float TURN_SPEED = 2.4f;   // radians / second
constexpr float CAM_UP = 3.2f;
constexpr float FOOT_OFFSET = 0.0f;

// ------------------------------------------------------------
// Init
// ------------------------------------------------------------
void walker_init(WalkerState& w)
{
    w.x = 0.0f;
    w.z = 0.0f;
    w.y = terrain_height(w.x, w.z) + FOOT_OFFSET;

    w.yaw = 0.0f;

    w.walk_phase = 0.0f;
    w.walk_speed = 0.0f;
}

// ------------------------------------------------------------
// Update
// ------------------------------------------------------------
void walker_update(
    WalkerState& w,
    const ControlState& ctl,
    float dt
)
{
    // --------------------------------------------------------
    // TURNING — DRIVEN BY LOOK (mouse + right stick)
    // --------------------------------------------------------
    w.yaw += ctl.look_dx * TURN_SPEED * dt;

    // wrap yaw into [0, 2π)
    if (w.yaw > 6.2831853f) w.yaw -= 6.2831853f;
    if (w.yaw < 0.0f)      w.yaw += 6.2831853f;

    // --------------------------------------------------------
    // MOVEMENT BASIS (FORWARD = FACING DIRECTION)
    // --------------------------------------------------------
    float cy = std::cos(w.yaw);
    float sy = std::sin(w.yaw);

    // forward vector
    float fwd_x = cy;
    float fwd_z = sy;

    // right vector (perpendicular)
    float right_x = -sy;
    float right_z = cy;

    // --------------------------------------------------------
    // MOVE
    // --------------------------------------------------------
    float move_fwd = ctl.forward;
    float move_side = ctl.strafe;

    w.x += (fwd_x * move_fwd + right_x * move_side) * MOVE_SPEED * dt;
    w.z += (fwd_z * move_fwd + right_z * move_side) * MOVE_SPEED * dt;

    // --------------------------------------------------------
    // GROUNDING
    // --------------------------------------------------------
    w.y = terrain_height(w.x, w.z) + FOOT_OFFSET;

    // --------------------------------------------------------
    // WALK ANIMATION DRIVER
    // --------------------------------------------------------
    w.walk_speed = std::fabs(move_fwd) + std::fabs(move_side);

    if (w.walk_speed > 0.01f) {
        w.walk_phase += w.walk_speed * 6.0f * dt;
        if (w.walk_phase > 6.2831853f)
            w.walk_phase -= 6.2831853f;
    }
}

// ------------------------------------------------------------
// Camera (locked behind walker, yaw-driven)
// ------------------------------------------------------------
void walker_update_camera(
    const WalkerState& w,
    Camera& cam,
    float cam_distance
)
{
    float cy = std::cos(w.yaw);
    float sy = std::sin(w.yaw);

    cam.target = {
        w.x,
        w.y + 1.6f,
        w.z
    };

    cam.pos = {
        w.x - cy * cam_distance,
        w.y + CAM_UP,
        w.z - sy * cam_distance
    };
}
