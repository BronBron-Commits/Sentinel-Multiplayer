#include <cstdio>
#include <unordered_map>
#include <arpa/inet.h>
#include <unistd.h>
#include <cmath>
#include <ctime>

#include "sentinel/net/protocol/snapshot.hpp"

static int sockfd = -1;
static uint32_t next_player_id = 1;

static std::unordered_map<uint64_t, uint32_t> addr_to_id;
static std::unordered_map<uint32_t, sockaddr_in> id_to_addr;
static std::unordered_map<uint32_t, Snapshot> players;

static uint64_t addr_key(const sockaddr_in& a) {
    return (uint64_t(a.sin_addr.s_addr) << 16) | ntohs(a.sin_port);
}

static double server_time() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(7777);
    bind(sockfd, (sockaddr*)&server, sizeof(server));

    printf("[server] listening on 0.0.0.0:7777\n");

    while (true) {
        Snapshot s{};
        sockaddr_in from{};
        socklen_t len = sizeof(from);

        ssize_t n = recvfrom(sockfd, &s, sizeof(s), 0, (sockaddr*)&from, &len);
        if (n != sizeof(s))
            continue;

        uint64_t key = addr_key(from);

        if (!addr_to_id.count(key)) {
            uint32_t id = next_player_id++;
            addr_to_id[key] = id;
            id_to_addr[id] = from;
            printf("[server] assigned id=%u\n", id);
        }

        uint32_t pid = addr_to_id[key];
        s.player_id   = pid;
        s.server_time = server_time();

        players[pid] = s;
        id_to_addr[pid] = from;

        for (const auto& [_, snap] : players) {
            for (const auto& [__, addr] : id_to_addr) {
                sendto(sockfd, &snap, sizeof(snap), 0,
                       (sockaddr*)&addr, sizeof(addr));
            }
        }
    }
}
