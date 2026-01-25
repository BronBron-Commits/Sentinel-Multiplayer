#include "sentinel/net/transport/udp_socket.hpp"

#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

class UdpSocketLinux final : public UdpSocket {
public:
    explicit UdpSocketLinux(uint16_t port) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            perror("socket");
            return;
        }

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(port);

        if (bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            close(sockfd);
            sockfd = -1;
        }
    }

    ~UdpSocketLinux() override {
        if (sockfd >= 0)
            close(sockfd);
    }

    // ------------------------------------------------------------
    // Address-explicit API (authoritative on Linux)
    // ------------------------------------------------------------

    ssize_t send_to(
        const void* data,
        size_t size,
        const sockaddr_in& to
    ) override {
        if (sockfd < 0)
            return -1;

        return sendto(
            sockfd,
            data,
            size,
            0,
            (const sockaddr*)&to,
            sizeof(to)
        );
    }

    ssize_t recv_from(
        void* out,
        size_t max,
        sockaddr_in& from
    ) override {
        if (sockfd < 0)
            return -1;

        socklen_t len = sizeof(from);
        ssize_t n = recvfrom(
            sockfd,
            out,
            max,
            0,
            (sockaddr*)&from,
            &len
        );

        if (n > 0) {
            last_peer = from; // remember for send_bytes
            has_peer  = true;
        }

        return n;
    }

    // ------------------------------------------------------------
    // Byte-stream compatibility API
    // ------------------------------------------------------------

    ssize_t send_bytes(
        const void* data,
        size_t size
    ) override {
        if (!has_peer)
            return -1;

        return send_to(data, size, last_peer);
    }

    ssize_t recv_bytes(
        void* out,
        size_t max
    ) override {
        sockaddr_in from{};
        ssize_t n = recv_from(out, max, from);
        return n;
    }

private:
    int sockfd = -1;

    sockaddr_in last_peer{};
    bool has_peer = false;
};

// ------------------------------------------------------------
// Factory
// ------------------------------------------------------------
UdpSocket* UdpSocket::create(const char*, uint16_t port) {
    return new UdpSocketLinux(port);
}

