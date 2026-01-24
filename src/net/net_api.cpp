#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

#include "sentinel/net/net_api.hpp"
#include "sentinel/net/protocol/snapshot.hpp"

static int sock = -1;
static sockaddr_in peer{};
static bool peer_known = false;

bool net_init(const char* host, uint16_t port) {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);

    if (strcmp(host, "0.0.0.0") == 0) {
        if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            return false;
        }
        printf("[net] bound %s:%u\n", host, port);
    } else {
        peer = addr;
        peer_known = true;
        printf("[net] client target %s:%u\n", host, port);
    }

    return true;
}

void net_shutdown() {
    if (sock >= 0)
        close(sock);
}

void net_send_snapshot(const Snapshot& s) {
    if (!peer_known) {
        printf("[net] no peer yet, not sending\n");
        return;
    }

    ssize_t n = sendto(
        sock,
        &s,
        sizeof(s),
        0,
        (sockaddr*)&peer,
        sizeof(peer)
    );

    if (n != sizeof(s))
        perror("sendto");
}

bool net_poll_snapshot(Snapshot& out) {
    sockaddr_in from{};
    socklen_t len = sizeof(from);

    ssize_t n = recvfrom(
        sock,
        &out,
        sizeof(out),
        MSG_DONTWAIT,
        (sockaddr*)&from,
        &len
    );

    if (n != sizeof(out))
        return false;

    // 🔑 Server learns client address here
    if (!peer_known) {
        peer = from;
        peer_known = true;
        printf("[net] learned peer %s:%u\n",
               inet_ntoa(from.sin_addr),
               ntohs(from.sin_port));
    }

    return true;
}
