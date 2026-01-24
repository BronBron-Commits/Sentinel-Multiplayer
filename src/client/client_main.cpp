#include <cstdio>
#include <thread>
#include <chrono>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"

int main() {
    setbuf(stdout, nullptr);
    net_init("127.0.0.1", 7777);

    PacketHeader hello{ PacketType::HELLO };
    net_send_raw(&hello, sizeof(hello));

    bool connected = false;
    uint32_t player_id = 0;

    float x = 0, z = 0, yaw = 0;
    uint32_t tick = 0;

    printf("[client] waiting for server...\n");

    while (true) {
        Snapshot s{};
        while (net_poll_snapshot(s)) {
            if (!connected && s.player_id != 0) {
                connected = true;
                player_id = s.player_id;
                x = s.x;
                z = s.z;
                yaw = s.yaw;
                printf("[client] connected as id=%u\n", player_id);
            }

            if (connected) {
                x += (s.x - x) * 0.1f;
                z += (s.z - z) * 0.1f;
                yaw += (s.yaw - yaw) * 0.1f;
            }
        }

        if (connected) {
            InputCmd in{};
            in.player_id = player_id;
            in.tick = tick++;
            in.throttle = 1.0f;
            in.yaw = 0.5f;
            in.pitch = 0.0f;

            net_send_raw(&in, sizeof(in));

            printf("[client] x=%.2f z=%.2f yaw=%.2f\n", x, z, yaw);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
