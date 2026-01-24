#pragma once
#include <cstdint>

enum class PacketType : uint32_t {
    HELLO = 1,
    SNAPSHOT = 2
};

struct Snapshot {
    PacketType type = PacketType::SNAPSHOT;

    uint32_t player_id = 0;
    uint32_t tick = 0;
    double   server_time = 0.0;

    float x = 0;
    float y = 0;
    float z = 0;
};
