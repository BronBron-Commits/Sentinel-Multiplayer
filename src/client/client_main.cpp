#include <cstdio>
#include <thread>
#include <chrono>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"

using SteadyClock = std::chrono::steady_clock;

int main() {
    setbuf(stdout, nullptr);
    net_init("127.0.0.1", 7777);

    // Send HELLO
    PacketHeader hello{ PacketType::HELLO };
    net_send_raw(&hello, sizeof(hello));

    bool connected = false;
    uint32_t player_id = 0;

    float predicted_x = 0.0f;
    float throttle = 1.0f;
    uint32_t tick = 0;

    printf("[client] waiting for server...\n");

    while (true) {
        Snapshot s{};
        while (net_poll_snapshot(s)) {
            if (!connected && s.player_id != 0) {
                connected = true;
                player_id = s.player_id;
                predicted_x = s.x;

                printf("[client] connected as id=%u\n", player_id);
            }

            if (connected) {
                float error = s.x - predicted_x;
                predicted_x += error * 0.1f;
            }
        }

        if (connected) {
            InputCmd in{};
            in.player_id = player_id;
            in.tick = tick++;
            in.throttle = throttle;

            net_send_raw(&in, sizeof(in));

            predicted_x += throttle * 2.0f * (1.0f / 60.0f);
            printf("[client] x=%.2f\n", predicted_x);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
