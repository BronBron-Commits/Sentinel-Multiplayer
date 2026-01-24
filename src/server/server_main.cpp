#include <cstdio>
#include <thread>
#include <chrono>
#include <unordered_map>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"
#include "sentinel/sim/sim_update.hpp"

using SteadyClock = std::chrono::steady_clock;
constexpr double TICK_DT = 1.0 / 60.0;

struct ClientState {
    SimPlayer player{};
    InputCmd  last_input{};
};

int main() {
    setbuf(stdout, nullptr);
    net_init("0.0.0.0", 7777);

    SimWorld world{};

    std::unordered_map<uint32_t, ClientState> clients;
    uint32_t next_player_id = 1;

    uint32_t tick = 0;

    const auto tick_dt =
        std::chrono::duration_cast<SteadyClock::duration>(
            std::chrono::duration<double>(TICK_DT)
        );

    auto next_tick = SteadyClock::now();
    printf("[server] running\n");

    while (true) {
        uint8_t buf[256];
        ssize_t n;

        // -------- RECEIVE --------
        while ((n = net_recv_raw(buf, sizeof(buf))) > 0) {
            auto* hdr = reinterpret_cast<PacketHeader*>(buf);

            if (hdr->type == PacketType::HELLO) {
                uint32_t id = next_player_id++;
                clients[id] = ClientState{};
                printf("[server] client joined: id=%u\n", id);

                Snapshot s{};
                s.player_id = id;
                net_send_snapshot(s);
            }
            else if (hdr->type == PacketType::INPUT) {
                auto* in = reinterpret_cast<InputCmd*>(buf);
                auto it = clients.find(in->player_id);
                if (it != clients.end()) {
                    it->second.last_input = *in;
                }
            }
        }

        // -------- SIMULATE --------
        for (auto& [id, c] : clients) {
            sim_update(
                world,
                c.player,
                TICK_DT,
                c.last_input.throttle,
                c.last_input.strafe,
                c.last_input.yaw,
                c.last_input.pitch
            );
        }

        // -------- BROADCAST --------
        for (auto& [id, c] : clients) {
            Snapshot s{};
            s.player_id = id;
            s.tick = tick;
            s.server_time = tick * TICK_DT;
            s.x = c.player.x;
            s.y = c.player.y;
            s.z = c.player.z;
            s.yaw = c.player.yaw;
            s.pitch = c.player.pitch;

            net_send_snapshot(s);
        }

        tick++;
        next_tick += tick_dt;
        std::this_thread::sleep_until(next_tick);
    }
}
