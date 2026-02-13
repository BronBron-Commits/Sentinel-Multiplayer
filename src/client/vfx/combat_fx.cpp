#include "render/render_stats.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <initializer_list>

#include <glad/glad.h>
#include <GL/glu.h>
#include <cmath>

// ------------------------------------------------------------
// Tuning constants (moved from client_main.cpp)
// ------------------------------------------------------------
constexpr float MISSILE_SPEED = 28.0f;
constexpr float MISSILE_LIFE = 4.0f;

constexpr float EXPLOSION_MAX_RADIUS = 6.0f;
constexpr float EXPLOSION_DURATION = 0.6f;

// ------------------------------------------------------------
// Internal state
// ------------------------------------------------------------
struct Missile {
    bool active = false;
    float x = 0, y = 0, z = 0;
    float vx = 0, vy = 0, vz = 0;
};

struct Explosion {
    bool active = false;
    float x = 0, y = 0, z = 0;
    float radius = 0;
    float time = 0;
};

static Missile   g_missile;
static Explosion g_explosion;

// ------------------------------------------------------------
// Internal helpers
// ------------------------------------------------------------
static void draw_missile()
{
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 0.7f, 0.2f);

    GL_BEGIN_WRAPPED(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(-0.8f, 0.0f, 0.0f);
    glEnd();

    glEnable(GL_LIGHTING);
}

static void draw_explosion_sphere(float radius, float alpha)
{
    const int LAT = 10;
    const int LON = 14;

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (int i = 0; i < LAT; ++i) {
        float t0 = float(i) / LAT;
        float t1 = float(i + 1) / LAT;

        float phi0 = (t0 - 0.5f) * 3.14159f;
        float phi1 = (t1 - 0.5f) * 3.14159f;

        GL_BEGIN_WRAPPED(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= LON; ++j) {
            float u = float(j) / LON;
            float theta = u * 6.28318f;

            for (float phi : { phi0, phi1 }) {
                float x = std::cos(phi) * std::cos(theta);
                float y = std::sin(phi);
                float z = std::cos(phi) * std::sin(theta);

                glColor4f(
                    1.0f,
                    0.6f - y * 0.3f,
                    0.2f,
                    alpha * (1.0f - t0)
                );

                glVertex3f(
                    x * radius,
                    y * radius,
                    z * radius
                );
            }
        }
        glEnd();
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
void combat_fx_init()
{
    g_missile = {};
    g_explosion = {};
}

void combat_fx_shutdown()
{
    // No resources to free (yet)
}

void combat_fx_update(float dt)
{
    if (g_missile.active) {
        g_missile.x += g_missile.vx * dt;
        g_missile.y += g_missile.vy * dt;
        g_missile.z += g_missile.vz * dt;
    }

    if (g_explosion.active) {
        g_explosion.time += dt;

        float t = g_explosion.time / EXPLOSION_DURATION;
        g_explosion.radius = EXPLOSION_MAX_RADIUS * t;

        if (g_explosion.time >= EXPLOSION_DURATION) {
            g_explosion.active = false;
        }
    }
}

void combat_fx_handle_fire(
    bool space_pressed,
    bool prev_space_pressed,
    float px,
    float py,
    float pz,
    float drone_yaw
) {
    if (space_pressed && !prev_space_pressed) {

        if (!g_missile.active) {
            // FIRE missile
            g_missile.active = true;

            g_missile.x = px + std::cos(drone_yaw) * 1.4f;
            g_missile.y = py + 0.15f;
            g_missile.z = pz + std::sin(drone_yaw) * 1.4f;

            g_missile.vx = std::cos(drone_yaw) * MISSILE_SPEED;
            g_missile.vy = 0.0f;
            g_missile.vz = std::sin(drone_yaw) * MISSILE_SPEED;
        }
        else {
            // DETONATE missile
            g_missile.active = false;

            g_explosion.active = true;
            g_explosion.x = g_missile.x;
            g_explosion.y = g_missile.y;
            g_explosion.z = g_missile.z;
            g_explosion.radius = 0.2f;
            g_explosion.time = 0.0f;
        }
    }
}

void combat_fx_render(float drone_yaw)
{
    if (g_missile.active) {
        glPushMatrix();
        glTranslatef(g_missile.x, g_missile.y, g_missile.z);
        glRotatef(drone_yaw * 57.2958f, 0, 1, 0);
        draw_missile();
        glPopMatrix();
    }

    if (g_explosion.active) {
        float t = g_explosion.time / EXPLOSION_DURATION;
        float alpha = 1.0f - t;

        glPushMatrix();
        glTranslatef(g_explosion.x, g_explosion.y, g_explosion.z);
        draw_explosion_sphere(g_explosion.radius, alpha);
        glPopMatrix();
    }
}
