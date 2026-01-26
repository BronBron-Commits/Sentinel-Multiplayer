#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <SDL.h>


#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>


// ✅ REQUIRED for std::remove_if

// ------------------------------------------------------------
// Simple firework particle stub
// ------------------------------------------------------------

struct FireworkParticle {
    float x, y, z;
    float vx, vy, vz;
    float life;
};

static std::vector<FireworkParticle> g_particles;

// ------------------------------------------------------------
// API
// ------------------------------------------------------------

void spawn_fireworks(float x, float y, float z, uint32_t seed) {
    std::srand(seed);

    constexpr int COUNT = 64;

    for (int i = 0; i < COUNT; ++i) {
        float theta = (float)std::rand() / RAND_MAX * 2.0f * 3.1415926f;
        float phi   = (float)std::rand() / RAND_MAX * 3.1415926f * 0.5f;

        float speed = 6.0f + ((float)std::rand() / RAND_MAX) * 4.0f;

        FireworkParticle p;
        p.x = x;
        p.y = y + 1.0f;
        p.z = z;

        p.vx = std::cos(theta) * std::cos(phi) * speed;
        p.vy = std::sin(phi) * speed;
        p.vz = std::sin(theta) * std::cos(phi) * speed;

        p.life = 2.5f;

        g_particles.push_back(p);
    }
}

void update_fireworks(float dt) {
    for (auto& p : g_particles) {
        p.vy -= 9.8f * dt;   // gravity
        p.x  += p.vx * dt;
        p.y  += p.vy * dt;
        p.z  += p.vz * dt;
        p.life -= dt;
    }

    g_particles.erase(
        std::remove_if(
            g_particles.begin(),
            g_particles.end(),
            [](const FireworkParticle& p) {
                return p.life <= 0.0f;
            }
        ),
        g_particles.end()
    );
}

void render_fireworks() {
    glDisable(GL_LIGHTING);
    glPointSize(3.0f);
    glBegin(GL_POINTS);

    for (const auto& p : g_particles) {
        float t = p.life / 2.5f;
        glColor3f(1.0f, t, 0.2f);
        glVertex3f(p.x, p.y, p.z);
    }

    glEnd();
}

