#pragma once

struct ControlState;
struct Camera;

struct WalkerState {
    // Position (same semantics as warthog)
    float x;
    float y;
    float z;

    float yaw;

    // Animation state
    float walk_phase;   // radians
    float walk_speed;   // derived from movement
};

void walker_init(WalkerState& w);

void walker_update(
    WalkerState& w,
    const ControlState& ctl,
    float dt
);

void walker_update_camera(
    const WalkerState& w,
    Camera& cam,
    float cam_distance
);
