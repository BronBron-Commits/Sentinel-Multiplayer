#include "input/control_system.hpp"
#include <SDL.h>
#include <cstring>

// ------------------------------------------------------------
// Global control state (one per client)
// ------------------------------------------------------------
static ControlState g_state;

// ------------------------------------------------------------
// Global control state (PRIVATE TO THIS FILE)
// ------------------------------------------------------------
static ControlState g_control{};

// ------------------------------------------------------------
// Init (called once at startup)
// ------------------------------------------------------------
void controls_init()
{
    std::memset(&g_state, 0, sizeof(g_state));
}

// ------------------------------------------------------------
// Keyboard state (called once per frame)
// ------------------------------------------------------------
void controls_update(bool ui_blocked)
{
    // Reset movement every frame
    g_state.forward = 0.0f;
    g_state.strafe = 0.0f;
    g_state.vertical = 0.0f;
    g_state.fire = false;
    g_state.boost = false;

    // If UI is blocking gameplay input, stop here
    if (ui_blocked)
        return;

    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    // Forward / backward (thrust)
    if (keys[SDL_SCANCODE_W]) g_state.forward += 1.0f;
    if (keys[SDL_SCANCODE_S]) g_state.forward -= 1.0f;

    // Strafe (optional, yaw-plane only)
    if (keys[SDL_SCANCODE_D]) g_state.strafe += 1.0f;
    if (keys[SDL_SCANCODE_A]) g_state.strafe -= 1.0f;

    // Vertical (legacy / optional)
    if (keys[SDL_SCANCODE_E]) g_state.vertical += 1.0f;
    if (keys[SDL_SCANCODE_Q]) g_state.vertical -= 1.0f;

    // Boost
    if (keys[SDL_SCANCODE_LSHIFT])
        g_state.boost = true;

    // Fire
    if (keys[SDL_SCANCODE_SPACE])
        g_state.fire = true;

    // ------------------------------------------------------------
    // Middle mouse button = roll modifier
    // ------------------------------------------------------------
    g_state.roll_modifier =
        (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
}


// ------------------------------------------------------------
// Mouse motion (called from SDL_PollEvent)
// ------------------------------------------------------------
void controls_on_mouse_motion(float dx, float dy)
{
    // Accumulate mouse deltas for THIS frame
    g_state.look_dx += dx;
    g_state.look_dy += dy;
}

void controls_on_mouse_wheel(float y)
{
    g_state.zoom_delta += y;
}


// ------------------------------------------------------------
// End of frame (called AFTER using look_dx/look_dy)
// ------------------------------------------------------------
void controls_end_frame()
{
    g_state.look_dx = 0.0f;
    g_state.look_dy = 0.0f;
    g_state.zoom_delta = 0.0f;
}


// ------------------------------------------------------------
// Access current control state
// ------------------------------------------------------------
const ControlState& controls_get()
{
    return g_state;
}

ControlState& controls_get_mutable()
{
    return g_control;
}
