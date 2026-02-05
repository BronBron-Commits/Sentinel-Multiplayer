#include "drone_controller.hpp"
#include "input/control_system.hpp"
#include "render/camera.hpp"

#include <cmath>
#include <algorithm>

constexpr float MOVE_SPEED = 9.0f;
constexpr float STRAFE_SPEED = 8.0f;

constexpr float CAM_UP = 4.5f;

void drone_init(DroneState& d)
{
    d.x = 0.0f;
    d.y = 1.5f;
    d.z = 0.0f;
    d.yaw = 0.0f;
    d.pitch = 0.0f;
    d.roll = 0.0f;
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

    if (std::fabs(ctl.forward) > 0.001f) {
        d.x += fwd_x * ctl.forward * MOVE_SPEED * speed_mul * dt;
        d.y += fwd_y * ctl.forward * MOVE_SPEED * speed_mul * dt;
        d.z += fwd_z * ctl.forward * MOVE_SPEED * speed_mul * dt;
    }

    if (std::fabs(ctl.strafe) > 0.001f) {
        d.x += right_x * ctl.strafe * STRAFE_SPEED * speed_mul * dt;
        d.z += right_z * ctl.strafe * STRAFE_SPEED * speed_mul * dt;
    }
}

void drone_update_camera(
    const DroneState& d,
    Camera& cam,
    float cam_distance
)
{
    float fwd_x = std::cos(d.yaw) * std::cos(d.pitch);
    float fwd_y = std::sin(d.pitch);
    float fwd_z = std::sin(d.yaw) * std::cos(d.pitch);

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
