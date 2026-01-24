#include <cstdio>
#include <thread>
#include <chrono>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"

int main() {
    std::puts("[server] starting");

    if (!net_init("0.0.0.0", 7777)) {
        std::puts("[server] net_init failed");
        return 1;
    }

    uint32_t tick = 0;

    while (true) {
        Snapshot s{};
        s.player_id = 1;
        s.x = tick * 0.05f;
        s.y = 0.0f;
        s.z = 0.0f;
        s.yaw = 0.0f;
        s.tick = tick++;

        net_send_snapshot(s);

        std::printf("[server] sent snapshot tick=%u x=%.2f\n",
                    s.tick, s.x);

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 Hz
    }

    net_shutdown();
    return 0;
}
