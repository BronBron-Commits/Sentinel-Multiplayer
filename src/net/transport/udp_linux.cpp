#include "sentinel/net/net_api.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>

int sock = -1;
sockaddr_in peer{};
static bool is_server = false;

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool net_init(const char* host, uint16_t port, bool server) {
    is_server = server;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return false;
    }

    set_nonblocking(sock);

    if (server) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            return false;
        }

        std::puts("[net] server bound");
    } else {
        peer.sin_family = AF_INET;
        peer.sin_port = htons(port);
        inet_pton(AF_INET, host, &peer.sin_addr);

        std::puts("[net] client ready");
    }

    return true;
}

void net_shutdown() {
    if (sock >= 0) close(sock);
    sock = -1;
}
