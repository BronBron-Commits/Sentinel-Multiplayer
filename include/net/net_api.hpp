#pragma once
#include <cstdint>

struct NetState {
    float x;
    float y;
    float z;
};

bool net_init(const char* addr, uint16_t port);
void net_shutdown();

bool net_tick(NetState& state);
bool net_send(const NetState& state);
