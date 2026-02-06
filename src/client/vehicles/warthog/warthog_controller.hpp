#pragma once
#include "input/control_system.hpp"
#include "render/camera.hpp"

struct WarthogState
{
    // World transform
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    // Orientation
    float yaw = 0.0f;

    // Forward speed (scalar)
    float speed = 0.0f;

    // --- NEW: vehicle dynamics ---
    float steer_angle = 0.0f; // steering wheel angle (radians)

    // World-space velocity (used for drift / slip)
    float vx = 0.0f;
    float vz = 0.0f;
};


void warthog_init(WarthogState& w);
void warthog_update(
    WarthogState& w,
    const ControlState& ctl,
    float dt
);
void warthog_update_camera(
    const WarthogState& w,
    Camera& cam,
    float cam_distance,
    float dt
);

