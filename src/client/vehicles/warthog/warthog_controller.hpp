#pragma once
#include "input/control_system.hpp"
#include "render/camera.hpp"

struct WarthogState {
    float x, y, z;
    float yaw;
    float speed;
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
    float cam_distance
);
