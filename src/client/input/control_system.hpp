#pragma once

struct ControlState
{
    // --- Mouse / camera look ---
    float look_dx = 0.0f;
    float look_dy = 0.0f;
    float zoom_delta = 0.0f;

    // --- Barrel roll modifier ---
    bool roll_modifier = false;

    // --- Movement ---
    float forward = 0.0f;
    float strafe = 0.0f;
    float vertical = 0.0f;

    bool fire = false;
    bool boost = false;
};

void controls_init();
void controls_update(bool ui_blocked = false);
void controls_on_mouse_motion(float dx, float dy);
void controls_on_mouse_wheel(float y);
void controls_end_frame();

const ControlState& controls_get();
