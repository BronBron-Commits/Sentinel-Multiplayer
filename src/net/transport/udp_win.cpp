#include "sentinel/net/transport/udp_socket.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdio>

class UdpSocketWin final : public UdpSocket {
    SOCKET sock = INVALID_SOCKET;

public:
    explicit UdpSocketWin(uint16_t port) {
        WSADATA wsa{};
        WSAStartup(MAKEWORD(2, 2), &wsa);

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        bind(sock, (sockaddr*)&addr, sizeof(addr));

        u_long nonblock = 1;
        ioctlsocket(sock, FIONBIO, &nonblock);

        printf("[net] Windows UDP bound :%u\n", port);
    }

    ~UdpSocketWin() override {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            WSACleanup();
        }
    }

    ssize_t send_bytes(const void*, size_t) override {
        return -1; // unused
    }

    ssize_t recv_bytes(void*, size_t) override {
        return -1; // unused
    }

    ssize_t send_to(const void* data, size_t size,
                    const sockaddr_in& to) override {
        return sendto(
            sock,
            (const char*)data,
            (int)size,
            0,
            (const sockaddr*)&to,
            sizeof(to)
        );
    }

    ssize_t recv_from(void* out, size_t max,
                      sockaddr_in& from) override {
        int from_len = sizeof(from);
        return recvfrom(
            sock,
            (char*)out,
            (int)max,
            0,
            (sockaddr*)&from,
            &from_len
        );
    }
};

UdpSocket* UdpSocket::create(const char*, uint16_t port) {
    return new UdpSocketWin(port);
}
