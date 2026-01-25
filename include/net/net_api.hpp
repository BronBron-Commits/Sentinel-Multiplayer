#pragma once

#include <cstdint>
#include <cstddef>
#include "net_event.hpp"

// ---------------- CONFIG ----------------

// Maximum player name length (including null terminator)
constexpr size_t NET_NAME_MAX = 32;

// Maximum chat message length
constexpr size_t NET_CHAT_MAX = 256;

// ---------------- STATE ----------------

// Single authoritative state packet.
// Sent at a fixed tick rate.
// Used for:
// - position
// - orientation
// - velocity (for smoothing / dead reckoning)
// - identity
// - chat (optional, transient)
struct NetState {
    uint32_t player_id;

    // World position
    float x;
    float y;
    float z;

    // Orientation (degrees)
    float yaw;
    float pitch;
    float roll;

    // Linear velocity (units / second)
    // Used ONLY for client-side smoothing
    float vx;
    float vy;
    float vz;

    // Identity (sent once or when changed)
    char name[NET_NAME_MAX];

    // Chat payload (empty string when unused)
    char chat[NET_CHAT_MAX];
};

// ---------------- LAYOUT SAFETY ----------------
//
// This assert ensures:
// - no accidental padding
// - Windows/Linux/MSVC/GCC compatibility
// - UDP packet size stability
//
static_assert(
    sizeof(NetState) ==
        sizeof(uint32_t) +          // player_id
        9 * sizeof(float) +         // pos (3) + rot (3) + vel (3)
        NET_NAME_MAX +
        NET_CHAT_MAX,
    "NetState layout mismatch"
);

// ---------------- API ----------------

// Initialize networking (client-side)
// addr: server IP (e.g. "127.0.0.1")
// port: server port (e.g. 7777)
bool net_init(const char* addr, uint16_t port);

// Send a single state packet (non-blocking)
bool net_send(const NetState& state);

// Poll for incoming state packets
// Returns true while packets are available
bool net_tick(NetState& out_state);

// Shutdown networking and release resources
void net_shutdown();

// ---------------- EVENT API ----------------

// Send a discrete event (fireworks, emotes, etc.)
bool net_send_event(const NetEvent& e);

// Poll for incoming events
bool net_tick_event(NetEvent& out_event);
