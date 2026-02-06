#include "warthog_controller.hpp"
#include <cmath>
#include <algorithm>
#include "render/render_terrain.hpp"

// ============================================================
// Vehicle tuning (FWD arcade car)
// ============================================================

constexpr float MAX_SPEED = 14.0f;
constexpr float ACCEL = 12.0f;
constexpr float BRAKE = 18.0f;

constexpr float MAX_STEER_RAD = 0.45f;
constexpr float STEER_SPEED = 6.0f;

constexpr float WHEEL_BASE = 3.2f;

// Longitudinal / lateral behavior
constexpr float BASE_GRIP = 9.0f;   // forward alignment
constexpr float SIDE_GRIP_FRONT = 5.0f;   // steering tires (slippy)
constexpr float SIDE_GRIP_REAR = 16.0f;  // stabilizing tires (strong)

// Camera
constexpr float CAM_HEIGHT = 8.5f;
constexpr float CAM_LOOK = 4.5f;
constexpr float CAM_AHEAD = 6.0f;
constexpr float CAM_LAG = 10.0f;

// Ride height
constexpr float GROUND_OFFSET = 1.25f;

// ============================================================
// Ground sampling
// ============================================================

static float sample_ground(const WarthogState& w)
{
    constexpr float HALF_W = 0.9f;
    constexpr float HALF_L = 1.2f;

    float fx = std::cos(w.yaw);
    float fz = std::sin(w.yaw);
    float rx = -fz;
    float rz = fx;

    float h0 = terrain_height(w.x + fx * HALF_L, w.z + fz * HALF_L);
    float h1 = terrain_height(w.x - fx * HALF_L, w.z - fz * HALF_L);
    float h2 = terrain_height(w.x + rx * HALF_W, w.z + rz * HALF_W);
    float h3 = terrain_height(w.x - rx * HALF_W, w.z - rz * HALF_W);

    return std::max(std::max(h0, h1), std::max(h2, h3));
}

// ============================================================
// Lifecycle
// ============================================================

void warthog_init(WarthogState& w)
{
    w = {};
    w.y = sample_ground(w) + GROUND_OFFSET;
}

void warthog_update(
    WarthogState& w,
    const ControlState& ctl,
    float dt
)
{
    // --------------------------------------------------------
    // 1. Steering (speed-reduced at high velocity)
    // --------------------------------------------------------
    float speed_factor =
        1.0f - std::clamp(std::abs(w.speed) / MAX_SPEED, 0.0f, 0.7f);

    float target_steer =
        ctl.strafe * MAX_STEER_RAD * speed_factor;

    w.steer_angle +=
        (target_steer - w.steer_angle) * STEER_SPEED * dt;

    // --------------------------------------------------------
    // 2. Engine / brake
    // --------------------------------------------------------
    float desired_speed = ctl.forward * MAX_SPEED;
    float accel =
        (std::abs(desired_speed) < std::abs(w.speed)) ? BRAKE : ACCEL;

    w.speed += (desired_speed - w.speed) * accel * dt;

    // --------------------------------------------------------
    // 3. Bicycle yaw (front-wheel steering)
    // --------------------------------------------------------
    if (std::abs(w.speed) > 0.1f)
    {
        float yaw_rate =
            (w.speed / WHEEL_BASE) * std::tan(w.steer_angle);
        w.yaw += yaw_rate * dt;
    }

    // --------------------------------------------------------
    // 4. Forward direction
    // --------------------------------------------------------
    float fx = std::sin(w.yaw);
    float fz = std::cos(w.yaw);

    // --------------------------------------------------------
    // 5. Target velocity (engine pulls front wheels)
    // --------------------------------------------------------
    float target_vx = fx * w.speed;
    float target_vz = fz * w.speed;

    w.vx += (target_vx - w.vx) * BASE_GRIP * dt;
    w.vz += (target_vz - w.vz) * BASE_GRIP * dt;

    // --------------------------------------------------------
    // 6. Lateral tire forces (FWD behavior)
    // --------------------------------------------------------
    float side_v = (-fz * w.vx + fx * w.vz);

    // Rear tires stabilize strongly
    w.vx -= (-fz) * side_v * SIDE_GRIP_REAR * dt;
    w.vz -= (fx)*side_v * SIDE_GRIP_REAR * dt;

    // Front tires allow slip (steering feel)
    w.vx -= (-fz) * side_v * SIDE_GRIP_FRONT * dt * 0.5f;
    w.vz -= (fx)*side_v * SIDE_GRIP_FRONT * dt * 0.5f;

    // --------------------------------------------------------
    // 7. Clamp energy
    // --------------------------------------------------------
    float vel_len = std::sqrt(w.vx * w.vx + w.vz * w.vz);
    float max_len = std::abs(w.speed);

    if (vel_len > max_len && vel_len > 0.0001f)
    {
        float s = max_len / vel_len;
        w.vx *= s;
        w.vz *= s;
    }

    // --------------------------------------------------------
    // 8. Integrate position
    // --------------------------------------------------------
    w.x += w.vx * dt;
    w.z += w.vz * dt;

    // --------------------------------------------------------
    // 9. Terrain following
    // --------------------------------------------------------
    float ground = sample_ground(w) + GROUND_OFFSET;
    w.y += (ground - w.y) * (1.0f - std::exp(-dt * 18.0f));
}

// ============================================================
// Camera — locked chase cam
// ============================================================

void warthog_update_camera(
    const WarthogState& w,
    Camera& cam,
    float cam_distance,
    float dt
)
{
    float fx = std::sin(w.yaw);
    float fz = std::cos(w.yaw);

    float desired_x = w.x - fx * cam_distance;
    float desired_y = w.y + CAM_HEIGHT;
    float desired_z = w.z - fz * cam_distance;

    float t = 1.0f - std::exp(-CAM_LAG * dt);

    cam.pos.x += (desired_x - cam.pos.x) * t;
    cam.pos.y += (desired_y - cam.pos.y) * t;
    cam.pos.z += (desired_z - cam.pos.z) * t;

    cam.target.x = w.x + fx * CAM_AHEAD;
    cam.target.y = w.y + CAM_LOOK;
    cam.target.z = w.z + fz * CAM_AHEAD;
}
