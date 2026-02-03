#include "client/input/control_system.hpp"
#include <SDL.h>

// ------------------------------------------------------------
// Internal state
// ------------------------------------------------------------
static ControlState g_state{};
static bool g_prev_fire = false;

// ------------------------------------------------------------
void controls_init() {
    g_state = {};
    g_prev_fire = false;
}

// ------------------------------------------------------------
void controls_update(bool chat_active) {
    SDL_PumpEvents();

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    if (chat_active) {
        keys = nullptr; // disable controls while typing
    }

    ControlState s{};

    if (keys) {
        if (keys[SDL_SCANCODE_UP])    s.forward += 1;
        if (keys[SDL_SCANCODE_DOWN])  s.forward -= 1;
        if (keys[SDL_SCANCODE_LEFT])  s.strafe -= 1;
        if (keys[SDL_SCANCODE_RIGHT]) s.strafe += 1;
        if (keys[SDL_SCANCODE_W])     s.vertical += 1;
        if (keys[SDL_SCANCODE_S])     s.vertical -= 1;
        if (keys[SDL_SCANCODE_A])     s.turn += 1;
        if (keys[SDL_SCANCODE_D])     s.turn -= 1;

        s.fire = keys[SDL_SCANCODE_SPACE];
    }

    s.fire_edge = s.fire && !g_prev_fire;
    g_prev_fire = s.fire;

    g_state = s;
}

// ------------------------------------------------------------
const ControlState& controls_get() {
    return g_state;
}
