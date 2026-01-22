#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <arpa/inet.h>
#include <unistd.h>

#include "net/net_api.hpp"

static int sockfd = -1;

/*
    We key clients by address (IP + port), not by player_id.
    This prevents multiple IDs being assigned to the same client.
*/
static uint32_t next_player_id = 1;

// addr_key -> player_id
static std::unordered_map<uint64_t, uint32_t> addr_to_id;

// player_id -> last known address
static std::unordered_map<uint32_t, sockaddr_in> id_to_addr;

// player_id -> last known state
static std::unordered_map<uint32_t, NetState> players;

static uint64_t addr_key(const sockaddr_in& a) {
    // Combine IP and port into a stable 64-bit key
    return (uint64_t(a.sin_addr.s_addr) << 16) | ntohs(a.sin_port);
}

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

    std::printf("[server] starting drone-server on 0.0.0.0:7777\n");

    while (true) {
        NetState state{};
        sockaddr_in from{};
        socklen_t from_len = sizeof(from);

        ssize_t n = recvfrom(
            sockfd,
            &state,
            sizeof(state),
            0,
            (sockaddr*)&from,
            &from_len
        );

        if (n <= 0)
            continue;

        uint64_t key = addr_key(from);

        // Assign ID ONCE per address
        if (!addr_to_id.count(key)) {
            uint32_t id = next_player_id++;
            addr_to_id[key] = id;
            id_to_addr[id] = from;

            std::printf("[server] assigned id=%u\n", id);
        }

        uint32_t player_id = addr_to_id[key];

        // Server is authoritative over player_id
        state.player_id = player_id;

        // Store last known state
        players[player_id] = state;
        id_to_addr[player_id] = from;

        // Broadcast this state to all connected clients
        for (const auto& [id, addr] : id_to_addr) {
            sendto(
                sockfd,
                &state,
                sizeof(state),
                0,
                (const sockaddr*)&addr,
                sizeof(addr)
            );
        }
    }

    close(sockfd);
    return 0;
}

