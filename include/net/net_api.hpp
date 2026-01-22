#pragma once
#include <cstdint>

struct NetState {
    uint32_t player_id;
    float x;
    float y;
    float z;
    float yaw;
};

// -------- networking API --------

// Initialize networking
// addr:
//   server -> "0.0.0.0"
//   client -> "127.0.0.1"
bool net_init(const char* addr, uint16_t port);

// Send one state packet
bool net_send(const NetState& state);

// Poll for incoming packets (non-blocking)
// Returns true if a packet was received
bool net_tick(NetState& out_state);

// Shutdown networking
void net_shutdown();

