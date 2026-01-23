#pragma once
#include <cstdint>
#include <cstddef>

// ---------------- CONFIG ----------------

constexpr size_t NET_NAME_MAX = 32;
constexpr size_t NET_CHAT_MAX = 256;

// ---------------- STATE ----------------

struct NetState {
    uint32_t player_id;

    float x;
    float y;
    float z;

    float yaw;
    float pitch;   // 🔹 ADD
    float roll;    // 🔹 ADD

    char name[NET_NAME_MAX];
    char chat[NET_CHAT_MAX];
};

// ---------------- API ----------------

bool net_init(const char* addr, uint16_t port);
bool net_send(const NetState& state);
bool net_tick(NetState& out_state);
void net_shutdown();

