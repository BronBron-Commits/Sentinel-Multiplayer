#include "sentinel/net/transport/udp_socket.hpp"
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

class UdpLinux final : public UdpSocket {
    int sock{};
    sockaddr_in peer{};
    bool has_peer = false;

public:
    UdpLinux(const char* host, uint16_t port) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        fcntl(sock, F_SETFL, O_NONBLOCK);

        // ---- bind locally ----
        sockaddr_in bind_addr{};
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;

        // Server binds fixed port, client binds ephemeral
        if (strcmp(host, "0.0.0.0") == 0) {
            bind_addr.sin_port = htons(port);   // server
        } else {
            bind_addr.sin_port = htons(0);      // client
        }

        bind(sock, (sockaddr*)&bind_addr, sizeof(bind_addr));

        // ---- preset peer (server address for client) ----
        memset(&peer, 0, sizeof(peer));
        peer.sin_family = AF_INET;
        peer.sin_port = htons(port);
        inet_pton(AF_INET, host, &peer.sin_addr);
    }

    ~UdpLinux() override {
        close(sock);
    }

    ssize_t send(const void* data, size_t size) override {
        if (!has_peer && peer.sin_addr.s_addr == 0)
            return -1;

        return sendto(sock, data, size, 0,
                      (sockaddr*)&peer, sizeof(peer));
    }

    ssize_t recv(void* out, size_t max) override {
        sockaddr_in from{};
        socklen_t len = sizeof(from);

        ssize_t n = recvfrom(
            sock,
            out,
            max,
            0,
            (sockaddr*)&from,
            &len
        );

        if (n > 0) {
            peer = from;
            has_peer = true;
        }

        return n;
    }
};

UdpSocket* UdpSocket::create(const char* host, uint16_t port) {
    return new UdpLinux(host, port);
}
