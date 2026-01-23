#pragma once

#include <cstdint>
#include <cstddef>
#include "net_event.hpp"   // ✅ ADD THIS

// ---------------- CONFIG ----------------

// Maximum player name length (including null terminator usage)
constexpr size_t NET_NAME_MAX = 32;

// Maximum chat message length
constexpr size_t NET_CHAT_MAX = 256;

// ---------------- STATE ----------------

// Single authoritative state packet.
// Sent at a fixed tick rate.
// Used for:
// - position
// - orientation
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

    // Identity (sent once or when changed)
    char name[NET_NAME_MAX];

    // Chat payload (empty string when unused)
    char chat[NET_CHAT_MAX];
};

// Layout safety check (optional but recommended)
static_assert(
    sizeof(NetState) ==
        sizeof(uint32_t) +
        6 * sizeof(float) +
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

// ---- EVENT API ----
bool net_send_event(const NetEvent& e);
bool net_tick_event(NetEvent& out_event);

