#pragma once

struct ControlState;
struct Camera;

struct DroneState {
    float x;
    float y;
    float z;

    float yaw;
    float pitch;
    float roll;
};

void drone_init(DroneState& d);

void drone_update(
    DroneState& d,
    const ControlState& ctl,
    float dt,
    float& camera_yaw
);

void drone_update_camera(
    const DroneState& d,
    Camera& cam,
    float cam_distance
);
