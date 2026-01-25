#pragma once
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using ssize_t = long long;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

class UdpSocket {
public:
    virtual ~UdpSocket() = default;

    virtual ssize_t send_bytes(const void* data, size_t size) = 0;
    virtual ssize_t recv_bytes(void* out, size_t max) = 0;

    virtual ssize_t send_to(const void* data, size_t size, const sockaddr_in& to) = 0;
    virtual ssize_t recv_from(void* out, size_t max, sockaddr_in& from) = 0;

    static UdpSocket* create(const char* host, uint16_t port);
};
