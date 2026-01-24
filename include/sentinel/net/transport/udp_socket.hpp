#pragma once
#include <cstddef>
#include <cstdint>
#include <sys/types.h>

class UdpSocket {
public:
    static UdpSocket* create(const char* host, uint16_t port);
    virtual ~UdpSocket() = default;

    virtual ssize_t send(const void* data, size_t size) = 0;
    virtual ssize_t recv(void* out, size_t max) = 0;
};
