#pragma once
#include "input/control_system.hpp"
#include "render/camera.hpp"

extern float warthog_cam_orbit;
extern bool  warthog_orbit_active;

struct WarthogState
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float yaw = 0.0f;
    float speed = 0.0f;
    float steer_angle = 0.0f;

    float vx = 0.0f;
    float vz = 0.0f;

    // Jumping
    float vy = 0.0f;
    bool on_ground = true;
    int jump_count = 0;
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
