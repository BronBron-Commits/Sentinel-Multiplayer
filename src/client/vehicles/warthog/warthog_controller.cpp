#include "warthog_controller.hpp"
#include <cmath>
#include <algorithm>
#include "render/render_terrain.hpp"

// ============================================================
// Tuning constants
// ============================================================

constexpr float MAX_SPEED = 14.0f;
constexpr float ACCEL = 10.0f;

// Mouse → steering only
constexpr float WARTHOG_YAW_SCALE = 0.0025f;

// Chase camera tuning
constexpr float WARTHOG_CAM_HEIGHT = 4.8f;
constexpr float WARTHOG_CAM_LOOK = 2.2f;
constexpr float WARTHOG_CAM_AHEAD = 6.0f;
constexpr float CAM_LAG = 12.0f;

// Ride height
static constexpr float WARTHOG_GROUND_OFFSET = 0.35f;

// ============================================================
// Ground sampling (4-point footprint)
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
    w.y = sample_ground(w) + WARTHOG_GROUND_OFFSET;
}

void warthog_update(
    WarthogState& w,
    const ControlState& ctl,
    float dt
)
{
    // --- steering (mouse X only, speed damped) ---
    float speed_factor =
        std::clamp(1.0f - std::abs(w.speed) / MAX_SPEED, 0.35f, 1.0f);

    w.yaw -= ctl.look_dx * WARTHOG_YAW_SCALE * speed_factor;

    // --- throttle ---
    float target_speed = ctl.forward * MAX_SPEED;
    w.speed += (target_speed - w.speed) * ACCEL * dt;

    float fx = std::cos(w.yaw);
    float fz = std::sin(w.yaw);

    w.x += fx * w.speed * dt;
    w.z += fz * w.speed * dt;

    // --- terrain following ---
    float ground = sample_ground(w);
    float target_y = ground + WARTHOG_GROUND_OFFSET;

    w.y += (target_y - w.y) * (1.0f - std::exp(-dt * 18.0f));
}

// ============================================================
// Camera (true chase cam)
// ============================================================

void warthog_update_camera(
    const WarthogState& w,
    Camera& cam,
    float cam_distance
)
{
    float fx = std::cos(w.yaw);
    float fz = std::sin(w.yaw);

    // Desired camera position
    float desired_x = w.x - fx * cam_distance;
    float desired_y = w.y + WARTHOG_CAM_HEIGHT;
    float desired_z = w.z - fz * cam_distance;

    // Smooth positional lag
    float t = 1.0f - std::exp(-CAM_LAG * 0.016f);

    cam.pos.x += (desired_x - cam.pos.x) * t;
    cam.pos.y += (desired_y - cam.pos.y) * t;
    cam.pos.z += (desired_z - cam.pos.z) * t;

    // Locked forward look
    cam.target.x = w.x + fx * WARTHOG_CAM_AHEAD;
    cam.target.y = w.y + WARTHOG_CAM_LOOK;
    cam.target.z = w.z + fz * WARTHOG_CAM_AHEAD;
}
