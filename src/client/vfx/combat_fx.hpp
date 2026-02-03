#pragma once

#include <cmath>

// Forward-declare only what we need
struct Camera;

// ------------------------------------------------------------
// Combat FX public API
// ------------------------------------------------------------

void combat_fx_init();
void combat_fx_shutdown();

void combat_fx_update(float dt);

// Handles missile fire / detonation logic
void combat_fx_handle_fire(
    bool space_pressed,
    bool prev_space_pressed,
    float px,
    float py,
    float pz,
    float drone_yaw
);

// Renders missiles + explosions
void combat_fx_render(float drone_yaw);
