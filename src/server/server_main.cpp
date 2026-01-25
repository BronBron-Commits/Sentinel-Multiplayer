#include <cstdio>
#include <thread>
#include <unordered_map>
#include <chrono>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"
#include "sentinel/sim/sim_update.hpp"

using Clock = std::chrono::steady_clock;
constexpr float DT = 1.0f / 60.0f;

struct Client {
    sockaddr_in addr{};
    SimPlayer   player{};
    SimPlayer   prev{};
    InputCmd    last_input{};
};

int main() {
    setbuf(stdout, nullptr);
    net_init("0.0.0.0", 7777);

    SimWorld world{};
    std::unordered_map<uint32_t, Client> clients;

    uint32_t next_id = 1;
    uint32_t tick = 0;

    auto next_tick = Clock::now();
    auto tick_step = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<float>(DT)
    );

    printf("[server] running\n");

    while (true) {
        uint8_t buf[256];
        sockaddr_in from{};
        ssize_t n;

        while ((n = net_recv_raw_from(buf, sizeof(buf), from)) > 0) {
            auto* hdr = (PacketHeader*)buf;

            if (hdr->type == PacketType::HELLO) {
                uint32_t id = next_id++;
                clients[id].addr = from;
                printf("[server] client joined id=%u\n", id);

                Snapshot s{};
                s.player_id = id;
                net_send_snapshot_to(s, from);
            }
            else if (hdr->type == PacketType::INPUT) {
                auto* in = (InputCmd*)buf;
                clients[in->player_id].last_input = *in;
            }
        }

        for (auto& [id, c] : clients) {
            c.prev = c.player;

            sim_update(
                world,
                c.player,
                DT,
                c.last_input.throttle,
                c.last_input.strafe,
                c.last_input.yaw,
                c.last_input.pitch
            );
        }

        for (auto& [id, c] : clients) {
            Snapshot s{};
            s.player_id = id;
            s.tick = tick;
            s.x = c.player.x;
            s.y = c.player.y;
            s.z = c.player.z;
            s.yaw = c.player.yaw;

            for (auto& [_, other] : clients)
                net_send_snapshot_to(s, other.addr);
        }

        tick++;
        next_tick += tick_step;
        std::this_thread::sleep_until(next_tick);
    }
}
