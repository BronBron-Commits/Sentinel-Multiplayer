#include "warthog_controller.hpp"

#include <cmath>
#include <algorithm>

#include "render/render_terrain.hpp"

// ============================================================
// Externs (defined in client_main.cpp)
// ============================================================

extern float warthog_cam_orbit;
extern bool  warthog_orbit_active;

// ============================================================
// Vehicle tuning (FWD arcade)
// ============================================================

constexpr float MAX_SPEED = 28.0f;
constexpr float ACCEL = 24.0f;
constexpr float BRAKE = 18.0f;

constexpr float MAX_STEER_RAD = 0.45f;
constexpr float STEER_SPEED = 2.2f; // Slower steering for Halo 3 feel

constexpr float WHEEL_BASE = 3.2f;

// Grip
constexpr float BASE_GRIP = 9.0f;
constexpr float SIDE_GRIP_FRONT = 5.0f;
constexpr float SIDE_GRIP_REAR = 16.0f;

// Camera
constexpr float CAM_HEIGHT = 17.0f;
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

    float fx = std::sin(w.yaw);
    float fz = std::cos(w.yaw);
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
    w.vy = 0.0f;
    w.on_ground = true;
    w.jump_count = 0;
}

void warthog_update(
    WarthogState& w,
    const ControlState& ctl,
    float dt
)
{
    // --- Walker-style camera-relative WASD movement and facing ---
    constexpr float MOUSE_SENS = 0.00180625f;
    float move_fwd = ctl.forward;
    float move_side = ctl.strafe;
    // Classic vehicle controls: W/S = forward/back, A/D = turn wheels
    // Mouse look (warthog_cam_orbit) only rotates the camera, not the vehicle's yaw or movement
    // Update pitch for sky/camera (like drone)
    w.pitch -= ctl.look_dy * MOUSE_SENS;
    w.pitch = std::clamp(w.pitch, -1.2f, 1.2f);
    float speed = move_fwd * MAX_SPEED;
    // Steering (A/D)
    float target_steer = -move_side * MAX_STEER_RAD;
    w.steer_angle += (target_steer - w.steer_angle) * STEER_SPEED * dt;
    // Update yaw based on steering and speed
    if (std::abs(speed) > 0.01f) {
        float yaw_rate = (speed / WHEEL_BASE) * std::tan(w.steer_angle);
        w.yaw += yaw_rate * dt;
    }
    // Move forward in current yaw (not camera-relative)
    float fx = std::sin(w.yaw);
    float fz = std::cos(w.yaw);
    w.x += fx * speed * dt;
    w.z += fz * speed * dt;
    // Model facing matches yaw
    w.visual_yaw = w.yaw;

    // visual_yaw and yaw are set by steering only; camera orbit does not affect vehicle facing
    w.visual_yaw = w.yaw;

    // --- Walker-style jump and gravity ---
    constexpr float GRAVITY = 32.0f;
    constexpr float JUMP_VELOCITY = 24.0f;
    float ground = sample_ground(w) + GROUND_OFFSET;
    static bool prev_jump = false;
    bool jump_pressed = ctl.fire;
    if (jump_pressed && !prev_jump) {
        if (w.on_ground || w.jump_count < 2) {
            w.vy = JUMP_VELOCITY;
            w.jump_count++;
            w.on_ground = false;
        }
    }
    prev_jump = jump_pressed;
    w.vy -= GRAVITY * dt;
    w.y += w.vy * dt;
    if (w.y <= ground) {
        w.y = ground;
        w.vy = 0.0f;
        if (!w.on_ground) {
            w.on_ground = true;
            w.jump_count = 0;
        }
    } else {
        w.on_ground = false;
    }
}

// ============================================================
// Camera — orbit-capable chase cam
// ============================================================

void warthog_update_camera(
    const WarthogState& w,
    Camera& cam,
    float cam_distance,
    float dt
)
{
    // Smoothly recenter when orbit not active (make lazier for Halo 3 feel)
    if (!warthog_orbit_active)
    {
        warthog_cam_orbit *= std::exp(-dt * 2.2f); // Slower recenter
    }

    // Camera orbits freely around warthog using warthog_cam_orbit
    float cam_yaw = w.yaw + warthog_cam_orbit;

    float fx = std::sin(cam_yaw);
    float fz = std::cos(cam_yaw);

    float desired_x = w.x - fx * cam_distance;
    float desired_y = w.y + CAM_HEIGHT;
    float desired_z = w.z - fz * cam_distance;

    float t = 1.0f - std::exp(-CAM_LAG * dt);

    cam.pos.x += (desired_x - cam.pos.x) * t;
    cam.pos.y += (desired_y - cam.pos.y) * t;
    cam.pos.z += (desired_z - cam.pos.z) * t;

    // Look where vehicle is facing, not orbit direction
    cam.target.x = w.x + std::sin(w.yaw) * CAM_AHEAD;
    cam.target.y = w.y + CAM_LOOK;
    cam.target.z = w.z + std::cos(w.yaw) * CAM_AHEAD;
}
