#include "net/net_api.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

static int sock_fd = -1;
static sockaddr_in peer_addr{};
static sockaddr_in last_sender{};
static socklen_t last_sender_len = sizeof(last_sender);
static bool has_peer = false;


bool net_init(const char* addr, uint16_t port) {
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return false;
    }

    sockaddr_in local{};
local.sin_family = AF_INET;
local.sin_addr.s_addr = INADDR_ANY;

/*
 * Client: bind to ephemeral port (0)
 * Server: bind to fixed port
 */
uint16_t bind_port =
    (std::strcmp(addr, "0.0.0.0") == 0) ? port : 0;

local.sin_port = htons(bind_port);

if (bind(sock_fd, (sockaddr*)&local, sizeof(local)) < 0) {
    perror("bind");
    close(sock_fd);
    sock_fd = -1;
    return false;
}


    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port);
    inet_pton(AF_INET, addr, &peer_addr.sin_addr);

    has_peer = false;
    return true;
}


void net_shutdown() {
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }
}

bool net_send(const NetState& state) {
    if (sock_fd < 0)
        return false;

    ssize_t sent = sendto(
        sock_fd,
        &state,
        sizeof(NetState),
        0,
        (sockaddr*)&peer_addr,
        sizeof(peer_addr)
    );

    return sent == sizeof(NetState);
}

bool net_tick(NetState& out_state) {
    if (sock_fd < 0)
        return false;

    NetState incoming{};
    ssize_t r = recvfrom(
        sock_fd,
        &incoming,
        sizeof(NetState),
        MSG_DONTWAIT,
        (sockaddr*)&last_sender,
        &last_sender_len
    );

    if (r == sizeof(NetState)) {
        has_peer = true;
        out_state = incoming;
        return true;
    }

    return false;
}

