#include <cstdio>
#include <thread>
#include <chrono>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"
#include "sentinel/net/replication/replication_client.hpp"

using SteadyClock = std::chrono::steady_clock;

int main() {
    setbuf(stdout, nullptr);
    printf("[client] starting\n");

    net_init("127.0.0.1", 7777);

    Snapshot hello{};
    hello.type = PacketType::HELLO;
    net_send_snapshot(hello);

    ReplicationClient repl;
    auto start = SteadyClock::now();

    while (true) {
        Snapshot s{};
        while (net_poll_snapshot(s)) {
            if (s.type == PacketType::SNAPSHOT) {
                repl.ingest(s);
            }
        }

        double render_time =
            std::chrono::duration<double>(
                SteadyClock::now() - start
            ).count() - 0.1;

        Snapshot a, b;
        if (repl.sample(0, render_time, a, b)) {
            double alpha =
                (render_time - a.server_time) /
                (b.server_time - a.server_time);

            double x = a.x + (b.x - a.x) * alpha;
            printf("[client] interp x=%.2f\n", x);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
