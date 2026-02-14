// Orbit state for walker camera (like warthog)
float walker_cam_orbit = 0.0f;
bool walker_orbit_active = false;
#include "walker_controller.hpp"
#include "input/control_system.hpp"
#include "render/camera.hpp"
#include "render/render_terrain.hpp"

#include <cmath>

// First-person camera for walker
void walker_update_camera_first_person(
    const WalkerState& w,
    Camera& cam
)
{
    float cy = std::cos(w.yaw);
    float sy = std::sin(w.yaw);
    // Camera at head position, looking forward
    cam.pos = { w.x, w.y + 1.6f, w.z };
    cam.target = { w.x + cy, w.y + 1.6f, w.z + sy };
}
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
    // Tank controls: forward/backward = move, left/right = rotate

    float move_fwd = ctl.forward;
    float move_side = ctl.strafe;
    // WASD directions are relative to camera (orbit)
    float cam_yaw = w.yaw + walker_cam_orbit;
    float cy = std::cos(cam_yaw);
    float sy = std::sin(cam_yaw);
    // Forward vector (camera forward)
    float fwd_x = cy;
    float fwd_z = sy;
    // Right vector (perpendicular)
    float right_x = -sy;
    float right_z = cy;
    w.x += (fwd_x * move_fwd + right_x * move_side) * MOVE_SPEED * dt;
    w.z += (fwd_z * move_fwd + right_z * move_side) * MOVE_SPEED * dt;

    // Grounding
    w.y = terrain_height(w.x, w.z) + FOOT_OFFSET;

    // Animation driver
    w.walk_speed = std::fabs(move_fwd);
    if (w.walk_speed > 0.01f) {
        w.walk_phase += w.walk_speed * 6.0f * dt;
        if (w.walk_phase > 6.2831853f)
            w.walk_phase -= 6.2831853f;
    }

    // Model facing matches yaw
    w.visual_yaw = w.yaw;
}

// ------------------------------------------------------------
// Camera (locked behind walker, yaw-driven)
// ------------------------------------------------------------
void walker_update_camera(
    const WalkerState& w,
    Camera& cam,
    float cam_height,
    float cam_distance,
    float cam_side_offset  // match .hpp
)
{
    // Orbit-capable chase cam (like warthog)
    float cam_yaw = w.yaw + walker_cam_orbit;
    float cy = std::cos(cam_yaw);
    float sy = std::sin(cam_yaw);
    cam.pos = {
        w.x - cy * cam_distance,
        w.y + cam_height,
        w.z - sy * cam_distance
    };
    cam.target = { w.x, w.y + 1.5f, w.z };
}
