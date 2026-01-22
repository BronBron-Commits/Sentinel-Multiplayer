#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <arpa/inet.h>
#include <unistd.h>

#include "net/net_api.hpp"

static int sockfd = -1;

struct Client {
    sockaddr_in addr{};
};

static std::unordered_map<uint32_t, Client> clients;
static uint32_t next_player_id = 1;

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

        // Assign player ID if needed
        if (state.player_id == 0) {
            state.player_id = next_player_id++;
            clients[state.player_id] = { from };
            std::printf("[server] assigned id=%u\n", state.player_id);
        } else {
            clients[state.player_id] = { from };
        }

        // Broadcast this state to ALL clients
        for (const auto& [id, client] : clients) {
            sendto(
                sockfd,
                &state,
                sizeof(state),
                0,
                (sockaddr*)&client.addr,
                sizeof(client.addr)
            );
        }
    }

    close(sockfd);
    return 0;
}
