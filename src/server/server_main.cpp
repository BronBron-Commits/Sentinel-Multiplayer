#include <cstdio>
#include <thread>
#include <chrono>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"
#include "sentinel/sim/sim_update.hpp"

using SteadyClock = std::chrono::steady_clock;
constexpr double TICK_DT = 1.0 / 60.0;

int main() {
    setbuf(stdout, nullptr);
    net_init("0.0.0.0", 7777);

    SimWorld world{};
    SimPlayer player{};

    bool has_client = false;
    uint32_t player_id = 0;
    InputCmd last_input{};

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

        while ((n = net_recv_raw(buf, sizeof(buf))) > 0) {
            auto* hdr = reinterpret_cast<PacketHeader*>(buf);

            if (hdr->type == PacketType::HELLO && !has_client) {
                has_client = true;
                player_id = 1;
                printf("[server] client joined: id=%u\n", player_id);

                Snapshot s{};
                s.player_id = player_id;
                net_send_snapshot(s);
            }
            else if (hdr->type == PacketType::INPUT) {
                last_input = *reinterpret_cast<InputCmd*>(buf);
            }
        }

        if (has_client) {
            // ✅ EXACTLY 5 floats after player
            sim_update(
                world,
                player,
                TICK_DT,
                last_input.throttle,
                last_input.strafe,
                last_input.yaw,
                last_input.pitch
            );

            Snapshot s{};
            s.player_id = player_id;
            s.tick = tick++;
            s.server_time = s.tick * TICK_DT;
            s.x = player.x;
            s.y = player.y;
            s.z = player.z;
            s.yaw = player.yaw;
            s.pitch = player.pitch;

            net_send_snapshot(s);
        }

        next_tick += tick_dt;
        std::this_thread::sleep_until(next_tick);
    }
}
