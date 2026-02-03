#pragma once
#include <cstdint>

// What the player wants to do THIS FRAME
struct ControlState {
    float forward = 0.0f;
    float strafe = 0.0f;
    float vertical = 0.0f;
    float turn = 0.0f;

    bool fire = false;   // held
    bool fire_edge = false;   // just pressed
};

void controls_init();
void controls_update(bool chat_active);
const ControlState& controls_get();
