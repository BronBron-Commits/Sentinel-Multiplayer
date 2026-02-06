#include "input/control_system.hpp"
#include <SDL_gamecontroller.h>

#include <SDL.h>
#include <cstring>
#include <cmath>

// ------------------------------------------------------------
// Rumble state
// ------------------------------------------------------------
static bool g_rumble_active = false;
static bool g_move_rumble_active = false;

// ------------------------------------------------------------
// Tuning
// ------------------------------------------------------------
constexpr float GAMEPAD_DEADZONE = 0.18f;
constexpr float GAMEPAD_LOOK_SENS = 100.0f;

// ------------------------------------------------------------
// Controller handle
// ------------------------------------------------------------
static SDL_GameController* g_controller = nullptr;

// ------------------------------------------------------------
// Global control state (SINGLE SOURCE OF TRUTH)
// ------------------------------------------------------------
static ControlState g_control{};

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static float apply_deadzone(float v)
{
    if (std::fabs(v) < GAMEPAD_DEADZONE)
        return 0.0f;

    return (v > 0.0f)
        ? (v - GAMEPAD_DEADZONE) / (1.0f - GAMEPAD_DEADZONE)
        : (v + GAMEPAD_DEADZONE) / (1.0f - GAMEPAD_DEADZONE);
}

// ------------------------------------------------------------
// Init
// ------------------------------------------------------------
void controls_init()
{
    std::memset(&g_control, 0, sizeof(g_control));
}

// ------------------------------------------------------------
// Per-frame update (keyboard + controller)
// ------------------------------------------------------------
void controls_update(bool ui_blocked)
{
    // Reset per-frame state
    g_control.forward = 0.0f;
    g_control.strafe = 0.0f;
    g_control.vertical = 0.0f;
    g_control.fire = false;
    g_control.boost = false;

    if (ui_blocked)
        return;

    // ------------------------------------------------------------
    // Keyboard
    // ------------------------------------------------------------
    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    if (keys[SDL_SCANCODE_W]) g_control.forward += 1.0f;
    if (keys[SDL_SCANCODE_S]) g_control.forward -= 1.0f;

    if (keys[SDL_SCANCODE_D]) g_control.strafe += 1.0f;
    if (keys[SDL_SCANCODE_A]) g_control.strafe -= 1.0f;

    if (keys[SDL_SCANCODE_E]) g_control.vertical += 1.0f;
    if (keys[SDL_SCANCODE_Q]) g_control.vertical -= 1.0f;

    if (keys[SDL_SCANCODE_LSHIFT])
        g_control.boost = true;

    if (keys[SDL_SCANCODE_SPACE])
        g_control.fire = true;

    g_control.roll_modifier =
        (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;

    // ------------------------------------------------------------
    // GameController (Xbox)
    // ------------------------------------------------------------
    if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0))
    {
        SDL_GameController* pad = SDL_GameControllerOpen(0);
        if (!pad)
            return;

        // --------------------------------------------------------
        // Left stick = movement
        // --------------------------------------------------------
        float lx = SDL_GameControllerGetAxis(
            pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
        float ly = SDL_GameControllerGetAxis(
            pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;

        lx = apply_deadzone(lx);
        ly = apply_deadzone(ly);

        g_control.strafe -= -lx;
        g_control.forward += -ly;

        // --------------------------------------------------------
        // Trigger (L2) = boost
        // --------------------------------------------------------
        float lt = SDL_GameControllerGetAxis(
            pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0f;

        bool boosting = (lt > 0.5f);
        g_control.boost = g_control.boost || boosting;

        // --------------------------------------------------------
        // Light rumble while moving (left stick)
        // --------------------------------------------------------
        bool moving =
            std::fabs(lx) > 0.01f ||
            std::fabs(ly) > 0.01f;

        if (moving && !boosting && !g_move_rumble_active)
        {
            SDL_GameControllerRumble(
                pad,
                2500,   // very light low freq
                1200,   // subtle texture
                120
            );
            g_move_rumble_active = true;
        }
        else if (!moving && g_move_rumble_active && !boosting)
        {
            SDL_GameControllerRumble(pad, 0, 0, 0);
            g_move_rumble_active = false;
        }

        // --------------------------------------------------------
        // Right stick = camera
        // --------------------------------------------------------
        float rx = SDL_GameControllerGetAxis(
            pad, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f;
        float ry = SDL_GameControllerGetAxis(
            pad, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;

        rx = apply_deadzone(rx);
        ry = apply_deadzone(ry);

        g_control.look_dx += rx * GAMEPAD_LOOK_SENS;
        g_control.look_dy += ry * GAMEPAD_LOOK_SENS;

        // --------------------------------------------------------
        // Rumble while boosting (override)
        // --------------------------------------------------------
        if (boosting)
        {
            SDL_GameControllerRumble(
                pad,
                12000,
                8000,
                100
            );

            g_rumble_active = true;
            g_move_rumble_active = false;
        }
    }
}

// ------------------------------------------------------------
// Mouse input (unchanged)
// ------------------------------------------------------------
void controls_on_mouse_motion(float dx, float dy)
{
    g_control.look_dx += dx;
    g_control.look_dy += dy;
}

void controls_on_mouse_wheel(float y)
{
    g_control.zoom_delta += y;
}

// ------------------------------------------------------------
// End of frame
// ------------------------------------------------------------
void controls_end_frame()
{
    g_control.look_dx = 0.0f;
    g_control.look_dy = 0.0f;
    g_control.zoom_delta = 0.0f;
}

// ------------------------------------------------------------
// Accessors
// ------------------------------------------------------------
const ControlState& controls_get()
{
    return g_control;
}

ControlState& controls_get_mutable()
{
    return g_control;
}

void controls_set_gamepad(SDL_GameController* pad)
{
    g_controller = pad;
}
