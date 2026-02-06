#include "drone_controller.hpp"
#include "input/control_system.hpp"
#include "render/camera.hpp"

#include <cmath>
#include <algorithm>

constexpr float MOVE_SPEED = 9.0f;
constexpr float STRAFE_SPEED = 8.0f;

constexpr float CAM_UP = 4.5f;

// ------------------------------------------------------------
// VISUAL-ONLY crane state (NOT part of DroneState)
// ------------------------------------------------------------
struct VisualCrane {
    float pitch = 0.0f; // forward/back dip
    float roll = 0.0f; // side dip (for rendering only)
};

static VisualCrane g_crane;

// Previous position for visual velocity
static float g_prev_x = 0.0f;
static float g_prev_y = 0.0f;
static float g_prev_z = 0.0f;

void drone_init(DroneState& d)
{
    d.x = 0.0f;
    d.y = 1.5f;
    d.z = 0.0f;

    d.yaw = 0.0f;
    d.pitch = 0.0f;
    d.roll = 0.0f;

    // Reset visual system
    g_prev_x = d.x;
    g_prev_y = d.y;
    g_prev_z = d.z;
    g_crane.pitch = 0.0f;
    g_crane.roll = 0.0f;
}

void drone_update(
    DroneState& d,
    const ControlState& ctl,
    float dt,
    float& camera_yaw
)
{
    constexpr float MOUSE_SENS = 0.00180625f;
    constexpr float ROLL_SENS = 0.00289f;

    // ------------------------------------------------------------
    // Rotation / input (UNCHANGED)
    // ------------------------------------------------------------
    if (ctl.roll_modifier) {
        float cam_right_sign = (std::cos(camera_yaw) >= 0.0f) ? 1.0f : -1.0f;
        d.roll += ctl.look_dx * ROLL_SENS * cam_right_sign;
    }
    else {
        camera_yaw += ctl.look_dx * MOUSE_SENS;
        d.yaw += ctl.look_dx * MOUSE_SENS;
    }

    d.pitch -= ctl.look_dy * MOUSE_SENS;

    d.roll = std::clamp(d.roll, -1.6f, 1.6f);
    d.pitch = std::clamp(d.pitch, -1.2f, 1.2f);

    if (!ctl.roll_modifier)
        d.roll *= 0.92f;

    float speed_mul = ctl.boost ? 2.0f : 1.0f;

    float fwd_x = std::cos(d.yaw) * std::cos(d.pitch);
    float fwd_y = std::sin(d.pitch);
    float fwd_z = std::sin(d.yaw) * std::cos(d.pitch);

    float right_x = -std::sin(camera_yaw);
    float right_z = std::cos(camera_yaw);

    // ------------------------------------------------------------
    // Movement (UNCHANGED)
    // ------------------------------------------------------------
    if (std::fabs(ctl.forward) > 0.001f) {
        d.x += fwd_x * ctl.forward * MOVE_SPEED * speed_mul * dt;
        d.y += fwd_y * ctl.forward * MOVE_SPEED * speed_mul * dt;
        d.z += fwd_z * ctl.forward * MOVE_SPEED * speed_mul * dt;
    }

    if (std::fabs(ctl.strafe) > 0.001f) {
        d.x += right_x * ctl.strafe * STRAFE_SPEED * speed_mul * dt;
        d.z += right_z * ctl.strafe * STRAFE_SPEED * speed_mul * dt;
    }

    // ------------------------------------------------------------
    // VISUAL-ONLY claw-machine behavior
    // ------------------------------------------------------------
    float inv_dt = (dt > 0.0001f) ? (1.0f / dt) : 0.0f;

    float vel_x = (d.x - g_prev_x) * inv_dt;
    float vel_z = (d.z - g_prev_z) * inv_dt;

    g_prev_x = d.x;
    g_prev_y = d.y;
    g_prev_z = d.z;

    // Dip toward direction of motion (world-space, facing-independent)
    float target_pitch = -vel_z * 0.030f;
    float target_roll = vel_x * 0.040f;

    g_crane.pitch += (target_pitch - g_crane.pitch) * 6.0f * dt;
    g_crane.roll += (target_roll - g_crane.roll) * 6.0f * dt;

    g_crane.pitch = std::clamp(g_crane.pitch, -0.35f, 0.35f);
    g_crane.roll = std::clamp(g_crane.roll, -0.45f, 0.45f);
}

void drone_update_camera(
    const DroneState& d,
    Camera& cam,
    float cam_distance
)
{
    // Apply visual pitch ONLY (camera has no roll)
    float pitch = d.pitch + g_crane.pitch;

    float fwd_x = std::cos(d.yaw) * std::cos(pitch);
    float fwd_y = std::sin(pitch);
    float fwd_z = std::sin(d.yaw) * std::cos(pitch);

    cam.target = {
        d.x + fwd_x,
        d.y + fwd_y,
        d.z + fwd_z
    };

    cam.pos = {
        d.x - fwd_x * cam_distance,
        d.y - fwd_y * cam_distance + CAM_UP,
        d.z - fwd_z * cam_distance
    };
}
