#include "sentinel/net/transport/udp_socket.hpp"

#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cstring>

class UdpSocketLinux final : public UdpSocket {
    int fd = -1;

public:
    explicit UdpSocketLinux(uint16_t port) {
        fd = socket(AF_INET, SOCK_DGRAM, 0);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        bind(fd, (sockaddr*)&addr, sizeof(addr));
        fcntl(fd, F_SETFL, O_NONBLOCK);
    }

    ~UdpSocketLinux() override {
        if (fd >= 0) close(fd);
    }

    ssize_t send_to(const void* data, size_t size,
                    const sockaddr_in& addr) override {
        return sendto(fd, data, size, 0,
            (const sockaddr*)&addr, sizeof(addr));
    }

    ssize_t recv_from(void* out, size_t max,
                      sockaddr_in& from) override {
        socklen_t len = sizeof(from);
        return recvfrom(fd, out, max, 0,
            (sockaddr*)&from, &len);
    }
};

UdpSocket* UdpSocket::create(const char*, uint16_t port) {
    return new UdpSocketLinux(port);
}
