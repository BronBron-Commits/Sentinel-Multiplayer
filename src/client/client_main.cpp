#include <cstdio>
#include <thread>
#include <chrono>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"

int main() {
    std::puts("[client] starting");

    if (!net_init("127.0.0.1", 7777)) {
        std::puts("[client] net_init failed");
        return 1;
    }

    while (true) {
        Snapshot s{};
        bool got = net_poll_snapshot(s);

        if (got) {
            std::printf(
                "[client] recv tick=%u player=%u x=%.2f\n",
                s.tick, s.player_id, s.x
            );
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    net_shutdown();
    return 0;
}
