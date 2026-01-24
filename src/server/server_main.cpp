#include <cstdio>
#include <thread>
#include <chrono>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"

int main() {
    setbuf(stdout, nullptr);

    printf("[server] starting\n");
    net_init("0.0.0.0", 7777);

    uint32_t tick = 0;

    while (true) {
        // 🔑 FIRST: poll to learn peer
        Snapshot incoming{};
        if (net_poll_snapshot(incoming)) {
            if (incoming.type == PacketType::HELLO) {
                printf("[server] received HELLO\n");
            }
        }

        // THEN: send snapshot (only works once peer known)
        Snapshot s{};
        s.type = PacketType::SNAPSHOT;
        s.tick = tick;
        s.x = tick * 0.05f;

        net_send_snapshot(s);

        printf("[server] tick=%u\n", tick);
        tick++;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
