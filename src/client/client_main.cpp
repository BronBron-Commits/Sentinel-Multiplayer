#include <cstdio>
#include <thread>
#include <chrono>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"

int main() {
    setbuf(stdout, nullptr);

    printf("[client] starting\n");

    net_init("127.0.0.1", 7777);

    // 🔑 Send HELLO
    Snapshot hello{};
    hello.type = PacketType::HELLO;
    net_send_snapshot(hello);

    printf("[client] hello sent\n");

    while (true) {
        Snapshot s;
        while (net_poll_snapshot(s)) {
            if (s.type == PacketType::SNAPSHOT) {
                printf("[client] recv tick=%u x=%.2f\n", s.tick, s.x);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
