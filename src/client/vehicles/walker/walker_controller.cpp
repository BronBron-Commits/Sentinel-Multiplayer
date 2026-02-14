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
    // --------------------------------------------------------
    // TURN WITH MOUSE, LOOK UP/DOWN WITH MOUSE
    // --------------------------------------------------------
    float move_fwd = ctl.forward;
    float move_side = ctl.strafe;

    // Mouse look: update yaw and pitch
    w.yaw += ctl.look_dx * 0.015f; // sensitivity
    w.pitch -= ctl.look_dy * 0.012f; // invert Y for natural look
    // Clamp pitch to avoid flipping
    if (w.pitch > 1.2f) w.pitch = 1.2f;
    if (w.pitch < -1.2f) w.pitch = -1.2f;

    // Wrap yaw
    if (w.yaw > 6.2831853f) w.yaw -= 6.2831853f;
    if (w.yaw < 0.0f)      w.yaw += 6.2831853f;

    float cy = std::cos(w.yaw);
    float sy = std::sin(w.yaw);

    // forward vector
    float fwd_x = cy;
    float fwd_z = sy;

    // right vector (perpendicular)
    float right_x = -sy;
    float right_z = cy;

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
    float cam_height,
    float cam_distance,
    float cam_side_offset  // match .hpp
)
{
    // Camera direction from yaw and pitch
    float cy = std::cos(w.yaw);
    float sy = std::sin(w.yaw);
    float cp = std::cos(w.pitch);
    float sp = std::sin(w.pitch);

    // Forward direction with pitch
    float fx = cy * cp;
    float fy = sp;
    float fz = sy * cp;

    // Camera target: look where head is facing
    cam.target = {
        w.x + fx,
        w.y + 1.6f + fy,
        w.z + fz
    };

    // Camera position: above and behind, optionally offset sideways
    cam.pos = {
        w.x - cy * cam_distance + (-sy) * cam_side_offset,
        w.y + cam_height,
        w.z - sy * cam_distance + cy * cam_side_offset
    };
}
