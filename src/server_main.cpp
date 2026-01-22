#include "net/net_api.hpp"

#include <cstdio>
#include <chrono>
#include <thread>

static NetState auth_state{};

int main() {
    std::printf("[server] starting drone-server on 0.0.0.0:7777\n");

    if (!net_init("0.0.0.0", 7777)) {
        std::fprintf(stderr, "[server] net_init failed\n");
        return 1;
    }

    constexpr float DT = 1.0f / 60.0f;

    while (true) {
        // Receive latest client state (temporary authority handoff)
        NetState incoming{};
        if (net_tick(incoming)) {
            auth_state = incoming; // authoritative overwrite
        }

        // Broadcast authoritative state back
        net_send(auth_state);

        std::this_thread::sleep_for(
            std::chrono::duration<float>(DT)
        );
    }

    net_shutdown();
    return 0;
}
