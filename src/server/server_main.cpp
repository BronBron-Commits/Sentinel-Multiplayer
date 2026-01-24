#include <cstdio>
#include <thread>
#include <chrono>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"

using SteadyClock = std::chrono::steady_clock;
constexpr double TICK_DT = 1.0 / 60.0;

int main() {
    setbuf(stdout, nullptr);

    printf("[server] starting\n");
    net_init("0.0.0.0", 7777);

    uint32_t tick = 0;

    const auto tick_duration =
        std::chrono::duration_cast<SteadyClock::duration>(
            std::chrono::duration<double>(TICK_DT)
        );

    auto next_tick = SteadyClock::now();

    while (true) {
        Snapshot incoming{};
        while (net_poll_snapshot(incoming)) {
            if (incoming.type == PacketType::HELLO) {
                printf("[server] received HELLO\n");
            }
        }

        Snapshot s{};
        s.type = PacketType::SNAPSHOT;
        s.tick = tick;
        s.server_time = tick * TICK_DT;
        s.x = tick * 0.05f;

        net_send_snapshot(s);

        tick++;
        next_tick += tick_duration;
        std::this_thread::sleep_until(next_tick);
    }
}
