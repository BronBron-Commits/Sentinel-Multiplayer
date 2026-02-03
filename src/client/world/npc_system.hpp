#pragma once

#include <cstdint>

// ------------------------------------------------------------
// NPC types
// ------------------------------------------------------------
enum class NPCType {
    DRONE,
    UFO
};

// ------------------------------------------------------------
// NPC data model (AUTHORITATIVE)
// ------------------------------------------------------------
struct NPC {
    NPCType type;

    float x, y, z;
    float yaw;
    float size;

    // Motion
    float vx, vy, vz;

    // AI state
    float target_x;
    float target_y;
    float target_z;
    float think_timer;
};

// ------------------------------------------------------------
// Limits
// ------------------------------------------------------------
constexpr int MAX_NPCS = 32;

// ------------------------------------------------------------
// NPC storage
// ------------------------------------------------------------
extern NPC npcs[MAX_NPCS];
extern int npc_count;

// ------------------------------------------------------------
// NPC API
// ------------------------------------------------------------
void npc_init();
void npc_update(float dt);

// ------------------------------------------------------------
// UFO particle system (NPC-owned)
// ------------------------------------------------------------
struct UFOParticle {
    float x, y, z;
    float vx, vy, vz;
    float life;
};

constexpr int UFO_PARTICLE_COUNT = 128;

extern UFOParticle ufo_particles[UFO_PARTICLE_COUNT];
