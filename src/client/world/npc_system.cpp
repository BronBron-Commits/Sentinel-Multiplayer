#include "npc_system.hpp"

#include <cmath>
#include <cstdlib>

// ------------------------------------------------------------
// Tuning
// ------------------------------------------------------------
constexpr float UFO_CRUISE_SPEED = 1.2f;
constexpr float UFO_STEER_RATE = 0.4f;
constexpr float UFO_DRIFT_DAMPING = 0.96f;

// ------------------------------------------------------------
// Storage (DEFINED ONCE)
// ------------------------------------------------------------
NPC npcs[MAX_NPCS];
int npc_count = 0;

UFOParticle ufo_particles[UFO_PARTICLE_COUNT];

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static float frand(float a, float b)
{
    return a + (b - a) * (float(rand()) / float(RAND_MAX));
}

// ------------------------------------------------------------
// API
// ------------------------------------------------------------
void npc_init()
{
    npc_count = 0;

    for (int i = 0; i < UFO_PARTICLE_COUNT; ++i)
        ufo_particles[i].life = 0.0f;
}

// ------------------------------------------------------------
// Spawning
// ------------------------------------------------------------
void spawn_ufo(float x, float y, float z, float size)
{
    if (npc_count >= MAX_NPCS)
        return;

    NPC& n = npcs[npc_count++];
    n.type = NPCType::UFO;

    n.x = x;
    n.y = y + 6.0f;
    n.z = z;

    n.vx = n.vy = n.vz = 0.0f;
    n.yaw = frand(0.0f, 6.2831853f);
    n.size = size;

    n.target_x = frand(-40.0f, 40.0f);
    n.target_y = frand(4.0f, 12.0f);
    n.target_z = frand(-40.0f, 40.0f);
    n.think_timer = frand(8.0f, 16.0f);
}

void spawn_drone(float x, float y, float z, float size)
{
    if (npc_count >= MAX_NPCS)
        return;

    NPC& n = npcs[npc_count++];
    n.type = NPCType::DRONE;

    n.x = x;
    n.y = y;
    n.z = z;

    n.vx = n.vy = n.vz = 0.0f;
    n.yaw = frand(0.0f, 6.2831853f);
    n.size = size;

    n.target_x = frand(-30.0f, 30.0f);
    n.target_y = frand(1.5f, 6.0f);
    n.target_z = frand(-30.0f, 30.0f);
    n.think_timer = frand(1.0f, 4.0f);
}

// ------------------------------------------------------------
// Update
// ------------------------------------------------------------
void npc_update(float dt)
{
    for (int i = 0; i < npc_count; ++i)
    {
        NPC& n = npcs[i];

        // Re-think
        n.think_timer -= dt;
        if (n.think_timer <= 0.0f)
        {
            n.target_x = frand(-50.0f, 50.0f);
            n.target_z = frand(-50.0f, 50.0f);

            n.target_y =
                (n.type == NPCType::UFO)
                ? frand(12.0f, 20.0f)
                : frand(1.5f, 6.0f);

            n.think_timer = frand(2.0f, 6.0f);
        }

        float dx = n.target_x - n.x;
        float dy = n.target_y - n.y;
        float dz = n.target_z - n.z;

        float dist = std::sqrt(dx * dx + dy * dy + dz * dz) + 0.001f;

        if (n.type == NPCType::UFO)
        {
            float tx = dx / dist * UFO_CRUISE_SPEED;
            float ty = dy / dist * UFO_CRUISE_SPEED;
            float tz = dz / dist * UFO_CRUISE_SPEED;

            n.vx += (tx - n.vx) * UFO_STEER_RATE * dt;
            n.vy += (ty - n.vy) * UFO_STEER_RATE * dt;
            n.vz += (tz - n.vz) * UFO_STEER_RATE * dt;

            n.vx *= UFO_DRIFT_DAMPING;
            n.vy *= UFO_DRIFT_DAMPING;
            n.vz *= UFO_DRIFT_DAMPING;

            n.x += n.vx * dt;
            n.y += n.vy * dt;
            n.z += n.vz * dt;

            n.yaw = std::atan2(n.vz, n.vx);
        }
        else
        {
            float speed = 5.0f;

            n.vx = dx / dist * speed;
            n.vy = dy / dist * speed;
            n.vz = dz / dist * speed;

            n.x += n.vx * dt;
            n.y += n.vy * dt;
            n.z += n.vz * dt;

            n.yaw = std::atan2(n.vz, n.vx);
        }
    }
}

// ------------------------------------------------------------
// UFO Particles
// ------------------------------------------------------------
void update_ufo_particles(float dt)
{
    for (int i = 0; i < UFO_PARTICLE_COUNT; ++i)
    {
        UFOParticle& p = ufo_particles[i];
        if (p.life <= 0.0f)
            continue;

        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;

        p.life -= dt * 1.2f;
        if (p.life < 0.0f)
            p.life = 0.0f;
    }
}
