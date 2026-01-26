#include <cstdio>
#include <unordered_map>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>
#include <cstring>

#include "sentinel/net/protocol/snapshot.hpp"
#include "sentinel/net/net_api.hpp"   // PacketHeader / PacketType

// ------------------------------------------------------------
// State
// ------------------------------------------------------------
static int sockfd = -1;
static uint32_t next_player_id = 1;

// addr_key -> player_id
static std::unordered_map<uint64_t, uint32_t> addr_to_id;

// player_id -> address
static std::unordered_map<uint32_t, sockaddr_in> id_to_addr;

// player_id -> last snapshot
static std::unordered_map<uint32_t, Snapshot> players;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static uint64_t addr_key(const sockaddr_in& a) {
    return (uint64_t(a.sin_addr.s_addr) << 16) | ntohs(a.sin_port);
}

static double server_time() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int main() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(7777);

    if (bind(sockfd, (sockaddr*)&server, sizeof(server)) < 0) {
        perror("bind");
        return 1;
    }

    printf("[server] listening on 0.0.0.0:7777\n");

    while (true) {
        uint8_t buffer[1024];
        sockaddr_in from{};
        socklen_t from_len = sizeof(from);

        ssize_t n = recvfrom(
            sockfd,
            buffer,
            sizeof(buffer),
            0,
            (sockaddr*)&from,
            &from_len
        );

        if (n <= 0)
            continue;

        uint64_t key = addr_key(from);

        // ----------------------------------------------------
        // HELLO PACKET
        // ----------------------------------------------------
        if (n == sizeof(PacketHeader)) {
            auto* hdr = reinterpret_cast<PacketHeader*>(buffer);

            if (hdr->type != PacketType::HELLO)
                continue;

            if (!addr_to_id.count(key)) {
                uint32_t id = next_player_id++;
                addr_to_id[key] = id;
                id_to_addr[id] = from;

                printf("[server] HELLO -> assigned id=%u\n", id);

                // Send initial snapshot so client learns ID
                Snapshot init{};
                init.player_id   = id;
                init.x = init.y = init.z = 0.0f;
                init.yaw         = 0.0f;
                init.server_time = server_time();

                players[id] = init;

                sendto(
                    sockfd,
                    &init,
                    sizeof(init),
                    0,
                    (sockaddr*)&from,
                    sizeof(from)
                );
            }

            continue;
        }

        // ----------------------------------------------------
        // SNAPSHOT PACKET
        // ----------------------------------------------------
        if (n == sizeof(Snapshot)) {
            Snapshot incoming{};
            std::memcpy(&incoming, buffer, sizeof(Snapshot));

            if (!addr_to_id.count(key))
                continue; // must HELLO first

            uint32_t pid = addr_to_id[key];

            incoming.player_id   = pid;
            incoming.server_time = server_time();

            players[pid] = incoming;
            id_to_addr[pid] = from;

            // Broadcast full world
            for (const auto& [_, snap] : players) {
                for (const auto& [__, addr] : id_to_addr) {
                    sendto(
                        sockfd,
                        &snap,
                        sizeof(snap),
                        0,
                        (sockaddr*)&addr,
                        sizeof(addr)
                    );
                }
            }

            continue;
        }
        
        // ----------------------------------------------------
// MISSILE PACKET
// ----------------------------------------------------
        if (n == sizeof(MissileSnapshot)) {
            MissileSnapshot ms{};
            std::memcpy(&ms, buffer, sizeof(MissileSnapshot));

            // must have HELLO'd
            if (!addr_to_id.count(key))
                continue;

            uint32_t pid = addr_to_id[key];

            // force authoritative owner id
            ms.owner_id = pid;

            // relay missile to all clients
            for (const auto& [_, addr] : id_to_addr) {
                sendto(
                    sockfd,
                    &ms,
                    sizeof(ms),
                    0,
                    (sockaddr*)&addr,
                    sizeof(addr)
                );
            }

            continue;
        }


        // Unknown packet → ignore
    }

    close(sockfd);
    return 0;
}

