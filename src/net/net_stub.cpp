// src/net/net_stub.cpp
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <queue>

#include "net/net_api.hpp"
#include "net/net_event.hpp"

// ------------------------------------------------------------
// Simple in-process stub (Windows / client-side)
// ------------------------------------------------------------

static std::queue<NetState> g_state_queue;
static std::queue<NetEvent> g_event_queue;

bool net_init(const char* host, uint16_t port) {
    std::printf("[net] stub init %s:%u\n", host, port);
    return true;
}

void net_shutdown() {
    while (!g_state_queue.empty()) g_state_queue.pop();
    while (!g_event_queue.empty()) g_event_queue.pop();
}

bool net_send(const NetState& state) {
    g_state_queue.push(state);
    return true;
}

bool net_tick(NetState& out) {
    if (g_state_queue.empty())
        return false;

    out = g_state_queue.front();
    g_state_queue.pop();
    return true;
}

bool net_send_event(const NetEvent& ev) {
    g_event_queue.push(ev);
    return true;
}

bool net_tick_event(NetEvent& out_event) {
    if (g_event_queue.empty())
        return false;

    out_event = g_event_queue.front();
    g_event_queue.pop();
    return true;
}
