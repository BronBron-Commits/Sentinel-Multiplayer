#include "sentinel/net/net_api.hpp"
#include <cstdio>

bool net_init(const char* host, uint16_t port) {
    std::printf("[net] init %s:%u\n", host, port);
    return true;
}

void net_shutdown() {
    std::printf("[net] shutdown\n");
}

void net_send_snapshot(const Snapshot&) {
    // server will implement later
}

bool net_poll_snapshot(Snapshot&) {
    return false;
}
