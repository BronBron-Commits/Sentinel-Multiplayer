#pragma once
#include <cstdint>

enum class PacketType : uint8_t {
    HELLO = 1,
    SNAPSHOT = 2,
    EVENT = 3,
    PING = 4,
    PONG = 5
};
