#pragma once
#include <cstdint>

enum class PacketType : uint32_t {
    HELLO    = 1,
    SNAPSHOT = 2,
    INPUT    = 3
};

struct PacketHeader {
    PacketType type;
};

struct InputCmd {
    PacketHeader hdr{ PacketType::INPUT };

    uint32_t player_id = 0;
    uint32_t tick = 0;

    float throttle = 0.0f;
    float strafe   = 0.0f;
    float yaw      = 0.0f;
    float pitch    = 0.0f;
};

struct Snapshot {
    PacketHeader hdr{ PacketType::SNAPSHOT };

    uint32_t player_id = 0;
    uint32_t tick = 0;
    double   server_time = 0.0;

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float yaw   = 0.0f;
    float pitch = 0.0f;

    // ---- DEAD RECKONING ----
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
};
