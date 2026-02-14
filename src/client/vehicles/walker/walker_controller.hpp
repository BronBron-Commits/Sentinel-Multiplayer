extern float walker_cam_orbit;
extern bool walker_orbit_active;
#pragma once

struct ControlState;
struct Camera;

struct WalkerState {
    // Position (same semantics as warthog)
    float x;
    float y;
    float z;

    float yaw;
    float pitch = 0.0f; // up/down look
    float visual_yaw = 0.0f; // for smooth model turning

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

// Add default values if you want
void walker_update_camera(
    const WalkerState& w,
    Camera& cam,
    float cam_height = 15.0f,
    float cam_distance = 25.0f,
    float cam_side_offset = 0.0f  // optional sideways offset
);

// First-person camera for walker
void walker_update_camera_first_person(
    const WalkerState& w,
    Camera& cam
);
