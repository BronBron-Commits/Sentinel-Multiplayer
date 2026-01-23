#pragma once
#include <cstdint>
#include <cstddef>   // ← REQUIRED for size_t

constexpr size_t NET_NAME_MAX = 32;


struct NetState {
    uint32_t player_id;
    float x;
    float y;
    float z;
    float yaw;
    char name[NET_NAME_MAX]; // OPTIONAL, sent once
};

// networking API
bool net_init(const char* addr, uint16_t port);
bool net_send(const NetState& state);
bool net_tick(NetState& out_state);
void net_shutdown();

