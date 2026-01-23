#include "net_api.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

// ---------------- INTERNAL ----------------

static int sock_fd = -1;
static sockaddr_in server_addr{};
static bool initialized = false;

// ---------------- API ----------------

bool net_init(const char* addr, uint16_t port) {
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return false;
    }

    // non-blocking
    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, addr, &server_addr.sin_addr) != 1) {
        perror("inet_pton");
        close(sock_fd);
        sock_fd = -1;
        return false;
    }

    initialized = true;
    return true;
}

bool net_send(const NetState& state) {
    if (!initialized)
        return false;

    ssize_t sent = sendto(
        sock_fd,
        &state,
        sizeof(NetState),
        0,
        reinterpret_cast<const sockaddr*>(&server_addr),
        sizeof(server_addr)
    );

    return sent == sizeof(NetState);
}

bool net_tick(NetState& out_state) {
    if (!initialized)
        return false;

    sockaddr_in from{};
    socklen_t from_len = sizeof(from);

    ssize_t received = recvfrom(
        sock_fd,
        &out_state,
        sizeof(NetState),
        0,
        reinterpret_cast<sockaddr*>(&from),
        &from_len
    );

    return received == sizeof(NetState);
}

void net_shutdown() {
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }
    initialized = false;
}

