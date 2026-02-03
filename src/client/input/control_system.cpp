#include "client/input/control_system.hpp"
#include <SDL.h>
#include <cstring>

// ------------------------------------------------------------
// Global control state
// ------------------------------------------------------------
static ControlState g_state;

// ------------------------------------------------------------
// Init
// ------------------------------------------------------------
void controls_init()
{
    std::memset(&g_state, 0, sizeof(g_state));
}

// ------------------------------------------------------------
// Keyboard state (movement NOT used for mouse-look test)
// ------------------------------------------------------------
void controls_update(bool /*ui_blocked*/)
{
    // Intentionally empty for now
}

// ------------------------------------------------------------
// Mouse motion (called from SDL_PollEvent loop)
// ------------------------------------------------------------
void controls_on_mouse_motion(float dx, float dy)
{
    g_state.look_dx += dx;
    g_state.look_dy += dy;
}

// ------------------------------------------------------------
// End of frame — clear deltas
// ------------------------------------------------------------
void controls_end_frame()
{
    g_state.look_dx = 0.0f;
    g_state.look_dy = 0.0f;
}

// ------------------------------------------------------------
// Access
// ------------------------------------------------------------
const ControlState& controls_get()
{
    return g_state;
}
