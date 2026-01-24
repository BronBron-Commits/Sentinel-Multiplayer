#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/types.h>   // ✅ ssize_t on Linux

class UdpSocket {
public:
    virtual ~UdpSocket() = default;

    virtual bool open(uint16_t port) = 0;
    virtual bool send(const void* data, size_t len,
                      const char* host, uint16_t port) = 0;

    // ssize_t is correct for recv semantics
    virtual ssize_t recv(void* out, size_t max) = 0;
};
