#include "sentinel/net/transport/udp_socket.hpp"
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

class UdpLinux final : public UdpSocket {
    int sock{};
    sockaddr_in peer{};

public:
    UdpLinux(const char* host, uint16_t port) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);

        fcntl(sock, F_SETFL, O_NONBLOCK);

        memset(&peer, 0, sizeof(peer));
        peer.sin_family = AF_INET;
        peer.sin_port = htons(port);
        inet_pton(AF_INET, host, &peer.sin_addr);

        sockaddr_in bind_addr{};
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        bind_addr.sin_port = htons(0); // EPHEMERAL PORT
        bind(sock, (sockaddr*)&bind_addr, sizeof(bind_addr));
    }

    ~UdpLinux() override {
        close(sock);
    }

    ssize_t send(const void* data, size_t size) override {
        return sendto(sock, data, size, 0,
                      (sockaddr*)&peer, sizeof(peer));
    }

    ssize_t recv(void* out, size_t max) override {
        return recvfrom(sock, out, max, 0, nullptr, nullptr);
    }
};

UdpSocket* UdpSocket::create(const char* host, uint16_t port) {
    return new UdpLinux(host, port);
}
